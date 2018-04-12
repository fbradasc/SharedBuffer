#include "ipc.h"

#include <sys/un.h>
#include <errno.h>

string IPC::_message;

#if defined(DEBUG_USERVER)
#define TRACE(format, ...)      printf("%s:%d - " format, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#define TRACE(format, ...)
#endif

/* size of control buffer to send/recv one file descriptor */
#define CONTROLLEN  CMSG_LEN(sizeof(int) )

IPC::IPC(): _sockfd(-1), _socket(-1)
{
}

IPC::~IPC()
{
    if (_sockfd >= 0)
    {
        close(_sockfd);

        _sockfd = -1;
    }

    if (_socket >= 0)
    {
        close(_socket);

        _socket = -1;
    }
}

bool IPC::put(const string & data)
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

bool IPC::get(std::string & data, size_t size)
{
    if (size <= 0)
    {
        TRACE("EXIT: invalid params!\n");

        return false;
    }

    char buffer[size];

    memset(&buffer[0], 0, sizeof(buffer));

    int retval = recv(_socket, buffer, size, 0);

    if (retval < 0)
    {
        TRACE("EXIT: receive failed!\n");

        return false;
    }

    buffer[size - 1] = '\0';

    data = buffer;

    TRACE("EXIT: %s\n", data.c_str());

    return true;
}

bool IPC::put(const int & data)
{
    TRACE("ENTER: %d\n", data);

    if (_socket != -1)
    {
        int retval = send(_socket, &data, sizeof(data), 0);

        if (retval < 0)
        {
            TRACE("EXIT: Send failed : %d - %s\n", retval, strerror(errno));

            return false;
        }
    }
    else
    {
        TRACE("Send failed : Socket not open\n");

        return false;
    }

    return true;
}

bool IPC::get(int & data)
{
    int retval;

    if (recv(_socket, &retval, sizeof(&retval), 0) < 0)
    {
        perror("EXIT: receive failed!\n");

        return false;
    }

    TRACE("EXIT: %d\n", retval);

    data = retval;

    return true;
}

bool IPC::putfd(const int & fd)
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

bool IPC::getfd(int & data)
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

        data = 0;

        return false;
    }

    if (retval < 0)
    {
        // I/O Exception
        //

        TRACE("EXIT: %s\n", strerror(errno));

        data = -1;

        return false;
    }

    if ( (msg.msg_flags & (MSG_CTRUNC | MSG_OOB | MSG_ERRQUEUE) ) != 0 )
    {
        // To us, any of the above flags are a fatal error
        //

        TRACE("EXIT: Fatal error: %s\n", strerror(errno));

        data = -1;

        return false;
    }

    if (msg.msg_controllen <= 0)
    {
        // Empty response
        //

        TRACE("EXIT: Empty response: %d\n", msg.msg_controllen);

        data = -1;

        return false;
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

            data = -1;

            return false;
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

    data = fd;

    return true;
}

/**********************************************************************************
 *                                                                                *
 *                               Server's stuffs                                  *
 *                                                                                *
 **********************************************************************************/

bool IPC::server_setup(const string & path)
{
    TRACE("ENTER: path=%s\n", path.c_str());

    if (_sockfd >= 0)
    {
        TRACE("EXIT: already opened - _sockfd=%d\n", _sockfd);

        return true;
    }

    unlink(path.c_str());

    _sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (_sockfd == -1)
    {
        perror("Could not create socket\n");

        return false;
    }

    memset(&_server, 0, sizeof(_server) );

    _server.sun_family = AF_UNIX;

    strcpy(_server.sun_path, path.c_str() );

    bind(_sockfd,
         (struct sockaddr *)&_server,
         strlen(_server.sun_path) + sizeof(_server.sun_family) );

    listen(_sockfd, 5);

    TRACE("EXIT: _sockfd=%d\n", _sockfd);

    return (_sockfd >= 0);
}

void* IPC::server_cb_(void *arg)
{
    int n;
    IPC *p = (IPC*)arg;

    char msg[MAXPACKETSIZE];

    pthread_detach(pthread_self());

    while ((n = recv(p->_socket, msg, MAXPACKETSIZE, 0)) != 0)
    {
        msg[n] = 0;

        p->_message = string(msg);

        TRACE("received: %s\n", p->_message.c_str());
    }

    msg[0] = 0;

    close(p->_socket);

    return NULL;
}

void IPC::server(void (*server_cb)(IPC *, void *), void *server_data)
{
    while (true)
    {
        socklen_t sosize = sizeof(_client);

        _socket = accept(_sockfd,
                         (struct sockaddr*)&_client,
                         &sosize);

        if (NULL != server_cb)
        {
            server_cb(this,server_data);

            close(_socket);
        }
        else
        {
            pthread_create(&_thread,
                           NULL,
                           &server_cb_,
                           (void *)this);
        }
    }
}

vector<string> IPC::parse(const char& separator)
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

    _message = "";

    return(result);
}

void IPC::detach()
{
    if (_socket >= 0)
    {
        close(_socket);

        _socket = -1;
    }

    if (_sockfd >= 0)
    {
        close(_sockfd);

        _sockfd = -1;
    }
}

/**********************************************************************************
 *                                                                                *
 *                               Client's stuffs                                  *
 *                                                                                *
 **********************************************************************************/

bool IPC::client_setup(const string & path)
{
    TRACE("ENTER: path=%s\n", path.c_str());

    if (_socket >= 0)
    {
        TRACE("EXIT: already opened - _socket=%d\n", _socket);

        return true;
    }

    _socket = socket(AF_UNIX, SOCK_STREAM, 0);

    if (_socket == -1)
    {
        perror("EXIT: Could not create socket\n");

        return false;
    }

    _server.sun_family = AF_UNIX;

    strcpy(_server.sun_path, path.c_str());

    int servlen = strlen(_server.sun_path) + sizeof(_server.sun_family);

    int retval = connect(_socket, (struct sockaddr *)&_server, servlen);

    if (retval < 0)
    {
        perror("EXIT: connect failed. Error");

        if (_socket >= 0)
        {
            close(_socket);

            _socket = -1;
        }
    }

    return connected();
}
