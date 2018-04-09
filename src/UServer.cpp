#include "UServer.h"

#include <sys/un.h>
#include <errno.h>

string UServer::_message;

#if defined(DEBUG_USERVER)
#define TRACE(format, ...)      printf("%s:%d - " format, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#define TRACE(format, ...)
#endif

/* size of control buffer to send/recv one file descriptor */
#define CONTROLLEN  CMSG_LEN(sizeof(int) )

void* UServer::task_(void *arg)
{
    int n;
    int sockfd = (long)arg;
    char msg[MAXPACKETSIZE];

    pthread_detach(pthread_self() );

    while (true)
    {
        n = recv(sockfd, msg, MAXPACKETSIZE, 0);

        if (n == 0)
        {
            close(sockfd);
            break;
        }

        msg[n] = 0;
        //send(sockfd,msg,n,0);
        _message = string(msg);

        TRACE("received: %s\n", _message.c_str());
    }

    return( 0);
}

UServer::UServer()
{
}

void UServer::setup(const string & path)
{
    unlink(path.c_str());

    _sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

    memset(&_server_addr, 0, sizeof(_server_addr) );

    _server_addr.sun_family = AF_UNIX;

    strcpy(_server_addr.sun_path, path.c_str() );

    bind(_sockfd,
         (struct sockaddr *)&_server_addr,
         strlen(_server_addr.sun_path) + sizeof(_server_addr.sun_family) );

    listen(_sockfd, 5);
}

void UServer::receive()
{
    while (true)
    {
        socklen_t sosize = sizeof(_client_addr);

        _sock = accept(_sockfd,
                       (struct sockaddr*)&_client_addr,
                       &sosize);

        pthread_create(&_server_thread,
                       NULL,
                       &task_,
                       (void *)_sock);
    }
}

vector<string> UServer::getMessage(const char& separator)
{
    string next;

    vector<string> result;

    // For each character in the string
    for (string::const_iterator it = _message.begin(); it != _message.end(); it++)
    {
        // If we've hit the terminal character
        if (*it == separator)
        {
            // If we have some characters accumulated
            if (!next.empty() )
            {
                TRACE("got: %s\n", next.c_str());
                // Add them to the result vector
                result.push_back(next);
                next.clear();
            }
        }
        else
        {
            // Accumulate the next character into the sequence
            next += *it;
        }
    }

    if (!next.empty() )
    {
        TRACE("got: %s\n", next.c_str());
        result.push_back(next);
    }

    clean();

    return(result);
}

bool UServer::send_fd(int fd)
{
    TRACE("INIT\n");

    ssize_t         retval                        = -1;
    unsigned char   payload                       = '*';
    char            buf[CMSG_SPACE(sizeof(int) )] = { 0 };
    struct iovec    iv                            = { 0 };
    struct msghdr   msg                           = { 0 };
    struct cmsghdr *cmsg                          = NULL;

    iv.iov_base        = &payload;
    iv.iov_len         = sizeof(payload);

    msg.msg_iov        = &iv;
    msg.msg_iovlen     = 1;
    msg.msg_control    = buf;
    msg.msg_controllen = sizeof(buf);

    cmsg               = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_len     = CMSG_LEN(sizeof(fd) );
    cmsg->cmsg_level   = SOL_SOCKET;
    cmsg->cmsg_type    = SCM_RIGHTS;

    *( (int *)CMSG_DATA(cmsg) ) = fd;

    msg.msg_controllen = cmsg->cmsg_len;

    do
    {
        TRACE("Sending msg_controllen: %d\n", msg.msg_controllen);

        retval = sendmsg(_sock, &msg, MSG_NOSIGNAL);

        TRACE("%d - %s\n", retval, strerror(errno));
    }
    while ( (retval < 0) && (errno == EINTR) );

    TRACE("EXIT: %d\n", retval);

    return(retval);
}

int UServer::recv_fd()
{
    TRACE("INIT\n");

    ssize_t       retval;
    struct msghdr msg          = { 0 };
    struct iovec  iv           = { 0 };
    unsigned char payload[256] = { 0 };
    int           fd           = -1;

    // Enough buffer for a pile of fd's.
    // We throw an exception if this buffer
    // is too small.
    unsigned char cmsgbuf[2 * sizeof(cmsghdr) + 0x100] = { 0 };

    do
    {
        iv.iov_base        = &payload;
        iv.iov_len         = sizeof(payload);

        msg.msg_iov        = &iv;
        msg.msg_iovlen     = 1;
        msg.msg_control    = cmsgbuf;

        msg.msg_controllen = sizeof(cmsgbuf);

        retval = recvmsg(_sock, &msg, MSG_NOSIGNAL);

        TRACE("Receiving: %d - %s\n", retval, strerror(errno));
    }
    while ((retval < 0) && (errno == EINTR));

    if ( (retval < 0) && (errno == EPIPE) )
    {
        // Treat this as an end of stream
        //

        TRACE("EXIT: End of stream\n");

        return(0);
    }

    if (retval < 0)
    {
        // I/O Exception
        //

        TRACE("EXIT: %s\n", strerror(errno));

        return(-1);
    }

    if ( (msg.msg_flags & (MSG_CTRUNC | MSG_OOB | MSG_ERRQUEUE) ) != 0 )
    {
        // To us, any of the above flags are a fatal error
        //

        TRACE("EXIT: Fatal error: %s\n", strerror(errno));

        return(-1);
    }

    if (msg.msg_controllen <= 0)
    {
        // Empty response
        //

        TRACE("EXIT: Empty response: %d\n", msg.msg_controllen);

        return(-1);
    }

    struct cmsghdr *pcmsgh = CMSG_FIRSTHDR(&msg);

    TRACE("Parsing: %p\n", pcmsgh);

    for (; pcmsgh != NULL; pcmsgh = CMSG_NXTHDR(&msg, pcmsgh) )
    {
        TRACE("Parsing: %p\n", pcmsgh);

        if (pcmsgh->cmsg_level != SOL_SOCKET)
        {
            TRACE("Skipping NOT SOL_SOCKETS\n");

            continue;
        }

        if (pcmsgh->cmsg_type != SCM_RIGHTS)
        {
            TRACE("Skipping NOT SCM_RIGHTS\n");

            continue;
        }

        const unsigned payload_len = pcmsgh->cmsg_len - CMSG_LEN(0);

        if ((payload_len % sizeof(fd)) != 0)
        {
            TRACE("Skipping\n");

            continue;
        }

        int *pDescriptors = (int *)CMSG_DATA(pcmsgh);

        int count = ( (pcmsgh->cmsg_len - CMSG_LEN(0) ) / sizeof(int) );

        TRACE("count=%d\n", count);

        if (count < 0)
        {
            // invalid cmsg length
            //
            TRACE("EXIT: invalid cmsg length\n");

            return -1;
        }

        for (int i = 0; i < count; i++)
        {
            TRACE("pDescriptors[%d]=%d\n", i, pDescriptors[i]);

            if (i==0)
            {
                fd = pDescriptors[i];
            }
        }
    }
    TRACE("EXIT: %d\n", fd);

    return(fd);
}

void UServer::send_iv(int val)
{
    TRACE("ENTER: %d\n", val);

    send(_sock, &val, sizeof(val), 0);
}

void UServer::clean()
{
    _message = "";
}

void UServer::detach()
{
    close(_sockfd);
    close(_sock);
}
