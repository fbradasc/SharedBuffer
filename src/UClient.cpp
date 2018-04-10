#include "UClient.h"

#include <errno.h>

#if defined(DEBUG_UCLIENT)
#define TRACE(format, ...)      printf("%s:%d - " format, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#define TRACE(format, ...)
#endif

/* size of control buffer to send/recv one file descriptor */
#define CONTROLLEN  CMSG_LEN(sizeof(int) )

UClient::UClient() :
    _socket(-1)
{
}

UClient::~UClient()
{
    detach();
}

bool UClient::attach(const string & path)
{
    if (_socket == -1)
    {
        _socket = socket(AF_UNIX, SOCK_STREAM, 0);

        if (_socket == -1)
        {
            cout << "Could not create socket" << endl;
        }
    }

    _server.sun_family = AF_UNIX;

    strcpy(_server.sun_path, path.c_str());

    int servlen = strlen(_server.sun_path) + sizeof(_server.sun_family);

    if (connect(_socket, (struct sockaddr *)&_server, servlen) < 0)
    {
        perror("connect failed. Error");

        detach();

        _socket = -1;
    }

    return connected();
}

bool UClient::send_tv(const string & data)
{
    TRACE("ENTER: %s\n", data.c_str());

    if (_socket != -1)
    {
        if (send(_socket, data.c_str(), strlen(data.c_str() ), 0) < 0)
        {
            cout << "Send failed : " << data << endl;
            return(false);
        }
    }
    else
    {
        return(false);
    }

    return(true);
}

string UClient::recv_tv(int size)
{
    if (size <= 0)
    {
        TRACE("EXIT: invalid params!\n");

        return "";
    }

    char buffer[size];

    memset(&buffer[0], 0, sizeof(buffer) );

    string reply;

    if (recv(_socket, buffer, size, 0) < 0)
    {
        TRACE("EXIT: receive failed!\n");

        return("");
    }

    buffer[size - 1] = '\0';
    reply = buffer;

    TRACE("EXIT: %s\n", reply.c_str());

    return(reply);
}

int UClient::recv_iv()
{
    int retval;

    if (recv(_socket, &retval, sizeof(&retval), 0) < 0)
    {
        TRACE("EXIT: receive failed!\n");

        return(false);
    }

    TRACE("EXIT: %d\n", retval);

    return(retval);
}

bool UClient::send_fd(int fd)
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

        retval = sendmsg(_socket, &msg, MSG_NOSIGNAL);

        TRACE("%d - %s\n", retval, strerror(errno));
    }
    while ( (retval < 0) && (errno == EINTR) );

    TRACE("EXIT: %d\n", retval);

    return(retval);
}

int UClient::recv_fd()
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

        retval = recvmsg(_socket, &msg, MSG_NOSIGNAL);

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

bool UClient::send_iv(int val)
{
    TRACE("ENTER: %d\n", val);

    if (_socket != -1)
    {
        if (send(_socket, &val, sizeof(val), 0) < 0)
        {
            cout << "Send failed : " << val << endl;

            return(false);
        }
    }
    else
    {
        return(false);
    }

    return(true);
}

void UClient::detach()
{
    if (_socket >= 0)
    {
        close(_socket);
    }
}

