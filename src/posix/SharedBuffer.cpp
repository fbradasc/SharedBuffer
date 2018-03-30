#include "MMSharedBuffer.h"

#include <iostream>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "posix_shm.h"

#define synchronize()           __sync_synchronize         ()
#define lock_test_and_set(P, V) __sync_lock_test_and_set   (&(P), (V) )
#define lock_release(P)         __sync_lock_release        (&(P) )

#if defined(DEBUG_SHB)
# define TRACE(format, ...)      printf("%s:%d - " format, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
# define TRACE(format, ...)
#endif

SharedBuffer::SharedBuffer() : _pshm(NULL), _owner(true), _grabbed(false)
{
}

SharedBuffer::~SharedBuffer()
{
    unmap();
}

void SharedBuffer::unmap()
{
    release();

    if (NULL != _pshm)
    {
        munmap(_pshm, _pshm->_size);
    }

    if (NULL != _name)
    {
        free(_name);
    }

    _pshm    = NULL;
    _owner   = false;
    _grabbed = false;
}

bool SharedBuffer::map(const char* name, size_t size, bool exclusive)
{
    return map_(name, size, exclusive);
}

bool SharedBuffer::grab()
{
    return grab_(true);
}

bool SharedBuffer::release()
{
    return grab_(false);
}

bool SharedBuffer::wait()
{
    if (NULL == _pshm)
    {
        return false;
    }

    while (lock_test_and_set(_pshm->_locks[_owner ? OTHER : OWNER], true) )
    {
    }

    return true;
}

bool SharedBuffer::notify()
{
    if (NULL == _pshm)
    {
        return false;
    }

    lock_release(_pshm->_locks[_owner ? OWNER : OTHER]);

    return true;
}

void SharedBuffer::dump()
{
    printf("Shared buffer name      : %d\n", name     () );
    printf("Shared buffer size      : %d\n", size     () );
    printf("Shared buffer grabbed   : %s\n", grabbed  () ? "true" : "false");
    printf("Shared buffer owner     : %s\n", owner    () ? "true" : "false");
    printf("Shared buffer exclusive : %s\n", exclusive() ? "true" : "false");
    printf("Shared buffer lock owner: %s\n", locked   (OWNER) ? "true" : "false");
    printf("Shared buffer lock other: %s\n", locked   (OTHER) ? "true" : "false");
    printf("Shared buffer lock third: %s\n", locked   (THIRD) ? "true" : "false");
}

bool SharedBuffer::grab_(bool grab)
{
    TRACE(" - ENTER\n");

    if (_owner)
    {
        TRACE(" - EXIT: owner\n");

        return true;
    }

    if (NULL == _pshm)
    {
        TRACE(" - EXIT: _pshm == NULL\n");

        return true; // avoid loops
    }

    synchronize();

    if (!_pshm->_exclusive)
    {
        TRACE(" - EXIT: !exclusive\n");

        return true;
    }

    if (grab)
    {
        _grabbed = !lock_test_and_set(_pshm->_locks[THIRD], true);

        TRACE(" - EXIT: grabbing=%d\n", _grabbed);
    }
    else
    if (_grabbed)
    {
        lock_release(_pshm->_locks[THIRD]);

        _grabbed = false;

        TRACE(" - EXIT: releasing\n");
    }

    TRACE(" - EXIT: grabbed=%d\n", _grabbed);

    return _grabbed;
}

bool SharedBuffer::map_(const char* name, size_t size, bool exclusive)
{
    TRACE("ENTER(name=%s, size=%d%s)\n", name, size, (exclusive) ? " exclusive" : "");

    if ( (NULL == name) || (0 == name[0]) )
    {
        TRACE("EXIT: invalid params\n");

        return false;
    }

    if ( (NULL != _pshm) && strcmp(name, _name) )
    {
        unmap();
    }

    int base_size = sizeof(SharedBufferPrivate);

    bool owner = (size > 0);

    int fd = -1;

    int flags = O_RDWR;

    // Try to open a named shared segment
    //
    if ( (fd = posix_shm_open(name, O_RDWR, 0666)) <= 0 )
    {
        TRACE("No shared segment found");

        // No shared segment found
        //
        if (!owner)
        {
            TRACE("EXIT: I'm not the owner\n");

            // I'm not the owner -> return
            //
            return false;
        }

        // I'm the owner -> create a new shared segment
        //
        if ( (fd = posix_shm_open(name, O_RDWR | O_CREAT, 0666) ) <= 0 )
        {
            TRACE("EXIT: Unable to create a new shared segment\n");

            // Unable to create a new shared segment -> return
            //
            return false;
        }

        if (posix_shm_truncate(fd, base_size) < 0)
        {
            TRACE("EXIT: Unable to set shared segment size\n");

            // Unable to set shared segment size
            //
            return false;
        }
    }

    // Let's map the shared segmet
    //
    SharedBufferPrivate *pshm = (SharedBufferPrivate *)mmap(NULL,
                                                            base_size,
                                                            PROT_READ | PROT_WRITE,
                                                            MAP_SHARED,
                                                            fd,
                                                            0);

    if (MAP_FAILED == (void *)pshm)
    {
        TRACE("EXIT: MAP_FAILED\n");

        return false;
    }

    if (NULL == pshm)
    {
        TRACE("EXIT: NULL == pshm\n");

        return false;
    }

    if (owner)
    {
        size += base_size;

        // The shared segment has been succesfully mapped,
        // make sure the size is valid
        //
        if (pshm->_size != size)
        {
            if (posix_shm_truncate(fd, size) < 0)
            {
                TRACE("EXIT: Unable to set shared segment size\n");

                // Unable to set shared segment size
                //
                return false;
            }
        }
    }
    else
    {
        size = pshm->_size;
    }

    munmap(pshm, base_size);

    pshm = (SharedBufferPrivate *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (MAP_FAILED == (void *)pshm)
    {
        TRACE("EXIT: MAP_FAILED\n");

        return false;
    }

    if (NULL == pshm)
    {
        TRACE("EXIT: NULL == pshm\n");

        return false;
    }

    _pshm = pshm;

    _name = strdup(name);

    if (owner)
    {
        _pshm->_size      = size;
        _pshm->_exclusive = exclusive;
    }

    _owner = owner;

    TRACE("EXIT: succesfully\n");

    return true;
}

bool SharedBuffer::destroy(const char *name)
{
    TRACE("ENTER: %s\n", name);

    if ( (NULL == name) || (0 == name[0]) )
    {
        TRACE("EXIT: invalid params\n");

        return false;
    }

    int retval = posix_shm_unlink(name);

    TRACE("EXIT: %d\n" retval);

    return (retval == 0);
}
