#if defined(__ANDROID__)

#include "UClient.h"
#include "posix_shm.h"
#include <linux/ashmem.h>

UClient ashamed;

#define _SS(a) stringstream(a)

#if defined(DEBUG_POSIX_SHM)
#define TRACE(format, ...)      printf("%s:%d - " format, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#define TRACE(format, ...)
#endif

#if defined(ANDROID)
#define SOCKET_PATH "/data/local/tmp/ion_socket"
#else
#define SOCKET_PATH "/tmp/ion_socket"
#endif

int posix_shm_truncate(int fd, size_t size)
{
    TRACE("ENTER\n");

    if (fd < 0)
    {
        TRACE("EXIT: %d\n");
        return(fd);
    }

    int retval = -1;

    if (ashamed.connected() )
    {
        TRACE("\n");

        ostringstream oss;

        oss << "T";
        oss << ":";
        oss << size;

        ashamed.send_tv(oss.str() );

        TRACE("\n");

        ashamed.send_fd(fd);

        TRACE("\n");

        retval = ashamed.recv_iv();
    }

    TRACE("EXIT: %d\n", retval);

    return(retval);
}

int posix_shm_unlink(const char* name)
{
    TRACE("ENTER\n");

    int retval = -1;

    if (ashamed.connected() )
    {
        TRACE("\n");

        ostringstream oss;

        oss << "U";
        oss << ":";
        oss << name;

        ashamed.send_tv(oss.str() );

        TRACE("\n");

        retval = ashamed.recv_iv();
    }

    TRACE("EXIT: %d\n", retval);

    return(retval);
}

int posix_shm_open(const char *name, int oflag, mode_t mode)
{
    TRACE("ENTER\n");

    int fd = -1;

    if (ashamed.setup(SOCKET_PATH))
    {
        TRACE("\n");

        ostringstream oss;

        oss << "O";
        oss << ":";
        oss << name;
        oss << ":";
        oss << oflag;
        oss << ":";
        oss << mode;

        ashamed.send_tv(oss.str() );

        TRACE("\n");

        fd = ashamed.recv_fd();
    }

    TRACE("EXIT: %d\n", fd);

    return(fd);
}

#endif
