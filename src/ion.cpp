#if defined(__ANDROID__)

#include "ion.h"

#include <stdio.h>

#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#if defined(DEBUG_ION)
#define TRACE(format, ...)      printf("%s:%d - " format, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#define TRACE(format, ...)
#endif

#define ION_IOCTRL_STR(v)                         \
   ( (v) == ION_IOC_ALLOC ) ? "ION_IOC_ALLOC" :   \
   ( (v) == ION_IOC_SHARE ) ? "ION_IOC_SHARE" :   \
   ( (v) == ION_IOC_FREE ) ? "ION_IOC_FREE" :     \
   ( (v) == ION_IOC_IMPORT ) ? "ION_IOC_IMPORT" : \
   ( (v) == ION_IOC_MAP ) ? "ION_IOC_MAP" : ""

ION::ION(): _fd(-1), _handle(0)
{
}

int ION::_ioctl(int req, void *arg)
{
    TRACE("ENTER: %s - %p\n", ION_IOCTRL_STR(req), arg);

    if (_fd < 0)
    {
        _fd = ::open("/dev/ion", O_RDONLY);

        if (_fd < 0)
        {
            TRACE("open /dev/ion failed!\n");
        }

        TRACE("Opened /dev/ion _fd=%d\n", _fd);
    }

    if (_fd < 0)
    {
        TRACE("Error opening /dev/ion: %s\n", strerror(errno) );

        return _fd;
    }

    int ret = ::ioctl(_fd, req, arg);

    if (ret < 0)
    {
        TRACE("ioctl %x failed with code %d: %s\n", req, ret, strerror(errno));

        return -errno;
    }

    return ret;

    TRACE("EXIT: %d\n", ret);

    return ret;
}

int ION::close()
{
    int ret = 0;

    if (_fd >= 0)
    {
        ret = ::close(_fd);
    }

    _fd = -1;

    return ret;
}

int ION::alloc(size_t len, size_t align, unsigned int heap_mask, unsigned int flags)
{
    int ret;
    struct ion_allocation_data data =
    {
        .len       = len      ,
        .align     = align    ,
        .heap_mask = heap_mask,
        .flags     = flags    ,
    };

    ret = _ioctl(ION_IOC_ALLOC, &data);

    if (ret < 0)
    {
        return ret;
    }

    _handle = data.handle;

    return ret;
}

int ION::free()
{
    struct ion_handle_data data =
    {
        .handle = _handle,
    };

    return _ioctl(ION_IOC_FREE, &data);
}

int ION::map(size_t length, int prot, int flags, off_t offset, void* &ptr, int &map_fd)
{
    struct ion_fd_data data =
    {
        .handle = _handle,
    };

    int ret = _ioctl(ION_IOC_MAP, &data);

    if (ret < 0)
    {
        return ret;
    }

    map_fd = data.fd;

    if (map_fd < 0)
    {
        TRACE("map ioctl returned negative fd\n");
        return -EINVAL;
    }

    ptr = mmap(NULL, length, prot, flags, map_fd, offset);

    if (ptr == MAP_FAILED)
    {
        TRACE("mmap failed: %s\n", strerror(errno) );
        return -errno;
    }

    return ret;
}

int ION::share(int &share_fd)
{
    struct ion_fd_data data =
    {
        .handle = _handle,
    };

    int ret = _ioctl(ION_IOC_SHARE, &data);

    if (ret < 0)
    {
        return ret;
    }

    share_fd = data.fd;

    if (share_fd < 0)
    {
        TRACE("share ioctl returned negative fd\n");
        return -EINVAL;
    }

    return ret;
}

int ION::alloc_fd(size_t len, size_t align, unsigned int heap_mask, unsigned int flags, int &handle_fd)
{
    struct ion_handle *handle;
    int ret;

    ret = alloc(len, align, heap_mask, flags);

    if (ret < 0)
    {
        return ret;
    }

    ret = share(handle_fd);

    // free();

    return ret;
}

int ION::import(int share_fd)
{
    struct ion_fd_data data =
    {
        .handle =        0,
        .fd     = share_fd,
    };

    int ret = _ioctl(ION_IOC_IMPORT, &data);

    if (ret < 0)
    {
        return ret;
    }

    _handle = data.handle;

    return ret;
}

int ION::sync_fd(int handle_fd)
{
    struct ion_fd_data data =
    {
        .handle =   _handle,
        .fd     = handle_fd,
    };

    return _ioctl(ION_IOC_SYNC, &data);
}

#endif
