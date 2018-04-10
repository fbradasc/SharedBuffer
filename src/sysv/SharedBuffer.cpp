#include "SharedBuffer.h"

#include <stdio.h>

#if defined(__ANDROID__)
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <errno.h>
#endif

#define synchronize()           __sync_synchronize         ()
#define lock_test_and_set(P, V) __sync_lock_test_and_set   (&(P), (V))
#define lock_release(P)         __sync_lock_release        (&(P))

#if defined(DEBUG_SHB)
#define TRACE(format, ...)      printf("%s:%d - " format, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#define TRACE(format, ...)
#endif

SharedBuffer::SharedBuffer() : _id(-1), _pshm(NULL), _owner(true), _grabbed(false)
{
}

SharedBuffer::~SharedBuffer()
{
    unmap();
}

void SharedBuffer::unmap()
{
    if (NULL != _pshm)
    {
        release();

        int retval = shmdt( (const void *)_pshm ); /* Detach the segment... */

        if (retval >= 0)
        {
            _pshm = NULL;
        }
    }
}

bool SharedBuffer::map(int id, size_t size, bool exclusive)
{
    return map_(id, size, exclusive);
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
    printf("Shared buffer id        : %d\n", id       ());
    printf("Shared buffer size      : %d\n", size     ()      );
    printf("Shared buffer grabbed   : %s\n", grabbed  ()      ? "true" : "false");
    printf("Shared buffer owner     : %s\n", owner    ()      ? "true" : "false");
    printf("Shared buffer exclusive : %s\n", exclusive()      ? "true" : "false");
    printf("Shared buffer lock owner: %s\n", locked   (OWNER) ? "true" : "false");
    printf("Shared buffer lock other: %s\n", locked   (OTHER) ? "true" : "false");
    printf("Shared buffer lock third: %s\n", locked   (THIRD) ? "true" : "false");
}

bool SharedBuffer::grab_(bool grab)
{
    TRACE("ENTER\n");

    if (_owner)
    {
        TRACE("EXIT: owner\n");

        return true;
    }

    if (NULL == _pshm)
    {
        TRACE("EXIT: _pshm == NULL\n");

        return true; // avoid loops
    }

    synchronize();

    if (!_pshm->_exclusive)
    {
        TRACE("EXIT: !exclusive\n");

        return true;
    }

    if (grab)
    {
        _grabbed = !lock_test_and_set(_pshm->_locks[THIRD], true);

        TRACE("EXIT: grabbing=%d\n", _grabbed);
    }
    else
    if (_grabbed)
    {
        lock_release(_pshm->_locks[THIRD]);

        _grabbed = false;

        TRACE("EXIT: releasing\n");
    }

    TRACE("EXIT: grabbed=%d\n", _grabbed);

    return _grabbed;
}

bool SharedBuffer::map_(int id, size_t size, bool exclusive)
{
    TRACE("ENTER(id=%d, size=%d%s)\n", id, size, (exclusive) ? " exclusive" : "");

    if ( (id < 0) && (size <= 0) )
    {
        TRACE("EXIT: invalid params\n");

        return false;
    }

    bool owner = (size > 0);

    SharedBufferPrivate *pshm = attach_(id); /* try to attach the segment... */

    if (owner)
    {
        TRACE("Owner\n");

        if (NULL != pshm)
        {
            // The shared segment has been succesfully attached,
            // make sure the size is the same
            //
            size = pshm->_size;

            TRACE("size=%d\n", size);
        }
        else
        {
            // The shared segment has not been succesfully attached,
            // try to create a new shared segment.
            //
            // Add room for control data (semaphores, buffer size, buffer anchor)
            //
            pshm = create_(id, size + sizeof(SharedBufferPrivate));
        }
    }

    if (NULL == pshm)
    {
        TRACE("EXIT: fail\n");

        return false;
    }

    _pshm = pshm;

    _id = id;

    if (owner)
    {
        TRACE("\n");

        _pshm->_size      = size;
        _pshm->_exclusive = exclusive;
    }

    _owner = owner;

    TRACE("EXIT: success\n");

    return true;
}

SharedBuffer::SharedBufferPrivate *SharedBuffer::attach_(int id)
{
    void *pshm = NULL;

    TRACE("ENTER: id=%d\n", id);

    if (id <= 0)
    {
        TRACE("EXIT: Invalid id: %d\n", id);

        return NULL;
    }

    pshm = shmat(id, NULL, 0); /* try to attach the segment... */

    TRACE("pshm=%p\n", pshm);

    if (MAP_FAILED == pshm)
    {
        TRACE("EXIT: MAP_FAILED\n");

        return NULL;
    }

    if (shmctl(id, IPC_RMID, NULL) < 0) /* ...and mark it destroyed. */
    {
        shmdt( (const void *)pshm ); /* Detach the segment... */

        TRACE("EXIT: fail\n");

        return NULL;
    }

    TRACE("EXIT: pshm=%p\n", pshm);

    return (SharedBufferPrivate *)pshm;
}

SharedBuffer::SharedBufferPrivate *SharedBuffer::create_(int & id, size_t size)
{
    TRACE("size=%d\n", size);

    id = shmget(IPC_PRIVATE,
                size + sizeof(SharedBufferPrivate),
                IPC_CREAT | SHM_R | SHM_W);

    void *pshm = attach_(id); /* try to attach the segment... */

    TRACE("EXIT: pshm=%p\n", pshm);

    return (SharedBufferPrivate *)pshm;
}

#if defined(__ANDROID__)
int SharedBuffer::shmctl(int __shmid, int __cmd, struct shmid_ds *__buf)
{
    TRACE("CALLED: %d, %d, %p\n", __shmid, __cmd, __buf);

    return 0;
}

int SharedBuffer::shmget(key_t __key, size_t __size, int __shmflg)
{
    TRACE("ENTER: %d, %d, %d\n", __key, __size, __shmflg);

    (void)__key;
    (void)__shmflg;

    int handle_fd=-1;

    int ret = _ion.alloc_fd(__size,
                            0,
                            ION_HEAP_CARVEOUT_MASK | ION_HEAP_SYSTEM_MASK,
                            ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC,
                            handle_fd);

    if (ret < 0)
    {
        TRACE("EXIT: %d\n", ret);

        return ret;
    }

    if (handle_fd < 0)
    {
        TRACE("ION_IOC_SHARE Failed: invalid fd=%d\n", handle_fd);

        ret = -EINVAL;
    }

    TRACE("fd: %d\n", handle_fd);

    void *ptr = NULL;

    ptr = mmap(NULL,
               sizeof(SharedBufferPrivate),
               PROT_READ | PROT_WRITE,
               MAP_SHARED,
               handle_fd,
               0);

    if (MAP_FAILED != ptr)
    {
        ( (SharedBufferPrivate*)ptr )->_size = __size;

        int err = munmap(ptr, sizeof(SharedBufferPrivate) );

        if (err < 0)
        {
            TRACE("UNMAP_FAILED sizeof(SharedBufferPrivate):%d - %s\n", sizeof(SharedBufferPrivate), strerror(errno) );
        }
    }
    else
    {
        TRACE("MAP_FAILED sizeof(SharedBufferPrivate):%d - %s\n", sizeof(SharedBufferPrivate), strerror(errno) );
    }

    TRACE("EXIT: %d\n", handle_fd);

    return handle_fd;
}

void *SharedBuffer::shmat(int __shmid, const void *__shmaddr, int __shmflg)
{
    TRACE("ENTER: %d, %p, %d\n", __shmid, __shmaddr, __shmflg);

    (void)__shmaddr;
    (void)__shmflg;

    void *ptr = NULL;

    if (__shmid < 0)
    {
        TRACE("EXIT: MAP_FAILED - Invalid __shmid<0\n");

        return MAP_FAILED;
    }

    int ret = -1;

    ret = _ion.import(__shmid);

    if (ret < 0)
    {
        TRACE("EXIT: MAP_FAILED - %d - %s\n", ret, strerror(errno) );

        return MAP_FAILED;
    }

    int shared_fd;

    ret = _ion.map(sizeof(SharedBufferPrivate),
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED,
                   0,
                   ptr,
                   shared_fd);

    if (ret < 0)
    {
        TRACE("EXIT: MAP_FAILED - %d - %s\n", ret, strerror(errno) );

        return MAP_FAILED;
    }

    TRACE("fd: %d - ptr=%p\n", shared_fd, ptr);

    if (shared_fd < 0)
    {
        TRACE("EXIT: MAP_FAILED - Invalid fd=%d\n", shared_fd);

        return MAP_FAILED;
    }

    if (MAP_FAILED == ptr)
    {
        TRACE("EXIT: MAP_FAILED sizeof(SharedBufferPrivate):%d - %s\n", sizeof(SharedBufferPrivate), strerror(errno) );

        return MAP_FAILED;
    }

    size_t size = ( (SharedBufferPrivate *)ptr )->_size;

    TRACE("Size: %d\n", size);

    ret = munmap(ptr, sizeof(SharedBufferPrivate) );

    TRACE("\n");

    if (ret < 0)
    {
        TRACE("EXIT: UNMAP_FAILED sizeof(SharedBufferPrivate):%d - %s\n", sizeof(SharedBufferPrivate), strerror(errno) );

        return MAP_FAILED;
    }

    ptr = mmap(NULL,
               size,
               PROT_READ | PROT_WRITE,
               MAP_SHARED,
               shared_fd,
               0);

    TRACE("\n");

    if (MAP_FAILED == ptr)
    {
        TRACE("EXIT: MAP_FAILED (%d): %s\n", size, strerror(errno) );

        return MAP_FAILED;
    }

    TRACE("EXIT: %p\n", ptr);

    return ptr;
}

int SharedBuffer::shmdt(const void *__shmaddr)
{
    TRACE("ENTER: %p\n", __shmaddr);

    int ret = 0;

    if (NULL != __shmaddr)
    {
        SharedBufferPrivate *pbuf = (SharedBufferPrivate *)__shmaddr;

        size_t size = pbuf->_size;

        ret = munmap( (void *)__shmaddr, size );

        if (ret < 0)
        {
            TRACE("EXIT: UNMAP_FAILED (%d) - %s\n", size, strerror(errno) );

            return ret;
        }
    }

    TRACE("EXIT: %d\n", ret);

    return ret;
}

#endif // defined(__ANDROID__)
