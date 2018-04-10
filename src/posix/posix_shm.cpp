#if defined(__ANDROID__)

#include "UServer.h"
#include "posix_shm.h"
#include <linux/ashmem.h>

UServer ashamed;

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

        ashamed.put(oss.str());

        TRACE("\n");

        ashamed.putfd(fd);

        TRACE("\n");

        ashamed.get(retval);
    }

    TRACE("EXIT: %d\n", retval);

    return(retval);
}

int posix_shm_unlink(const char* name)
{
    TRACE("ENTER\n");

    int retval = -1;

    if (ashamed.connected())
    {
        TRACE("\n");

        ostringstream oss;

        oss << "U";
        oss << ":";
        oss << name;

        ashamed.put(oss.str());

        TRACE("\n");

        ashamed.get(retval);
    }

    TRACE("EXIT: %d\n", retval);

    return(retval);
}

int posix_shm_open(const char *name, int oflag, mode_t mode)
{
    TRACE("ENTER\n");

    int fd = -1;

    if (ashamed.client_setup(SOCKET_PATH))
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

        ashamed.put(oss.str());

        TRACE("\n");

        ashamed.getfd(fd);
    }

    TRACE("EXIT: %d\n", fd);

    return(fd);
}

#endif
