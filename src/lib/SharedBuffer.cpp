#include "SharedBuffer.h"

// #include <iostream>
// #include <sstream>
#include <string>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>    /* For O_* constants */
#include <sys/stat.h> /* For mode constants */
#include <sys/types.h>

#include "ipc.h"

#define synchronize()           __sync_synchronize         ()
#define lock_test_and_set(P, V) __sync_lock_test_and_set   (&(P), (V) )
#define lock_release(P)         __sync_lock_release        (&(P) )

#if defined(DEBUG_SHB)
# define TRACE(format, ...)      printf("%s:%d - " format, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
# define TRACE(format, ...)
#endif

#define STRINGCAT(a,b) std::string(a) + std::string(b)
#if defined(USE_SYSV_SHM) || defined(ANDROID)
#if defined(ANDROID)
#define SHARED_BUFFER_NAME(n) STRINGCAT("/data/local/tmp/", n)
#else
#define SHARED_BUFFER_NAME(n) STRINGCAT("/tmp/", n)
#endif
#else
#define SHARED_BUFFER_NAME(n) STRINGCAT("/", n)
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

bool SharedBuffer::wait(const int timeout_ms)
{
    if (NULL == _pshm)
    {
        return false;
    }

    int count_down = timeout_ms;

    while (lock_test_and_set(_pshm->_locks[_owner ? OTHER : OWNER], true))
    {
        if (count_down == 0)
        {
            return false;
        }
        else
        if (count_down > 0)
        {
            count_down--;

            usleep(1000); // wait 1 ms
        }
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
    printf("Shared buffer name      : %s\n", name     () );
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

bool SharedBuffer::map_(const char* name, size_t size, bool exclusive)
{
    TRACE("ENTER(name=%s, size=%d%s)\n", name, size, (exclusive) ? " exclusive" : "");

    int fd = -1;

    if ( (NULL == name) || (0 == name[0]) )
    {
        TRACE("EXIT: invalid params\n");

        return false;
    }

    if ( (NULL != _pshm) && strcmp(name, _name) )
    {
        unmap();
    }

    bool owner = (size > 0);

    SharedBufferPrivate *pshm = attach_(name); /* try to attach the segment... */

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
            pshm = create_(name, size + sizeof(SharedBufferPrivate), fd);
        }
    }

    if (NULL == pshm)
    {
        TRACE("EXIT: fail\n");

        return false;
    }

    _pshm = pshm;

    _name = strdup(name);

    if (owner)
    {
        TRACE("\n");

        _pshm->_size      = size;
        _pshm->_exclusive = exclusive;

        _fd = fd;

#if defined(USE_SYSV_SHM) || defined(ANDROID)
        pthread_create(&_owner_thread, NULL, publisher_, (void *)this);
#endif

    }

    _owner = owner;

    TRACE("EXIT: success\n");

    return true;
}

SharedBuffer::SharedBufferPrivate *SharedBuffer::attach_(const char *name)
{
    void *pshm = NULL;

    TRACE("ENTER: name=%s\n", name);

    if ( (NULL == name) || (0 == name[0]) )
    {
        TRACE("EXIT: invalid params\n");

        return NULL;
    }

    std::string buffer_name = SHARED_BUFFER_NAME(name);

#if defined(USE_SYSV_SHM) || defined(ANDROID)
    int id = retrieve_buffer_id_(buffer_name);

    if ( id <= 0 )
    {
        TRACE("EXIT: Unable to get the shared segment id\n");

        return NULL;
    }

    // try to attach the segment...
    //
    pshm = shm_attach_(id);
#else
    // Try to open a named shared segment
    //
    int fd = shm_open(buffer_name.c_str(), O_RDWR, 0666);

    if ( fd <= 0 )
    {
        TRACE("No shared segment found");

        return NULL;
    }

    size_t base_size = sizeof(SharedBufferPrivate);

    // Let's map the shared segmet
    //
    pshm = mmap(NULL,
                base_size,
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                fd,
                0);

    TRACE("pshm=%p\n", pshm);

    if (MAP_FAILED == pshm)
    {
        TRACE("EXIT: MAP_FAILED\n");

        return NULL;
    }

    if (NULL == pshm)
    {
        TRACE("EXIT: NULL == pshm\n");

        return NULL;
    }

    size_t size = ((SharedBufferPrivate*)pshm)->_size;

    munmap(pshm, base_size);

    pshm = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
#endif

    TRACE("pshm=%p\n", pshm);

    if (MAP_FAILED == pshm)
    {
        TRACE("EXIT: MAP_FAILED\n");

        return NULL;
    }

    if (NULL == pshm)
    {
        TRACE("EXIT: NULL == pshm\n");

        return NULL;
    }

    return (SharedBufferPrivate *)pshm;
}

SharedBuffer::SharedBufferPrivate *SharedBuffer::create_(const char *name, size_t size, int & fd)
{
    void *pshm = NULL;

    TRACE("ENTER: name=%s, size=%d\n", name, size);

    if ( (NULL == name) || (0 == name[0]) )
    {
        TRACE("EXIT: invalid params\n");

        return NULL;
    }

    fd = -1;

    std::string buffer_name = SHARED_BUFFER_NAME(name);

#if defined(USE_SYSV_SHM) || defined(ANDROID)
    fd = shmget(IPC_PRIVATE,
                size,
                IPC_CREAT | SHM_R | SHM_W);

    if ( fd <= 0 )
    {
        TRACE("EXIT: Unable to create a new shared segment\n");

        return NULL;
    }

    // try to attach the segment...
    //
    pshm = shm_attach_(fd);
#else
    // I'm the owner -> create a new shared segment
    //
    fd = shm_open(buffer_name.c_str(), O_RDWR | O_CREAT, 0666);

    if ( fd <= 0 )
    {
        TRACE("EXIT: Unable to create a new shared segment\n");

        return NULL;
    }

    if (ftruncate(fd, size) < 0)
    {
        TRACE("EXIT: Unable to set shared segment size\n");

        return NULL;
    }

    // Let's map the shared segmet
    //
    pshm = mmap(NULL,
                size,
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                fd,
                0);
#endif

    if (MAP_FAILED == pshm)
    {
        TRACE("EXIT: MAP_FAILED\n");

        return NULL;
    }

    if (NULL == pshm)
    {
        TRACE("EXIT: NULL == pshm\n");

        return NULL;
    }

    return (SharedBufferPrivate *)pshm;
}

bool SharedBuffer::destroy(const char *name)
{
    TRACE("ENTER: %s\n", name);

    if ( (NULL == name) || (0 == name[0]) )
    {
        TRACE("EXIT: invalid params\n");

        return false;
    }

    int retval = 0;

#if !defined(USE_SYSV_SHM) && !defined(ANDROID)
    retval = shm_unlink(name);
#endif

    TRACE("EXIT: %d\n", retval);

    return (retval == 0);
}

#if defined(USE_SYSV_SHM) || defined(ANDROID)
void SharedBuffer::publish_buffer_id_(IPC *ipc, void *data)
{
    TRACE("ENTER: ipc=%p, data=%p\n", ipc, data);

    if (NULL == ipc)
    {
        TRACE("EXIT: invalid params\n");

        return;
    }

    int id = (intptr_t)data;

    bool success = false;

#if defined(ANDROID)
    success = ipc->putfd(id);
#else
    success = ipc->put(id);
#endif

    TRACE("EXIT: %s\n", ((success) ? "success" : "failure"));
}

void *SharedBuffer::publisher_(void * m)
{
    TRACE("ENTER: m=%p\n", m);

    if (NULL == m)
    {
        TRACE("EXIT: invalid params\n");

        return NULL;
    }

    pthread_detach(pthread_self());

    pthread_t msg;

    IPC ipc;

    SharedBuffer *sb = (SharedBuffer*)m;

    ipc.server_setup(SHARED_BUFFER_NAME(sb->_name));

    ipc.server(publish_buffer_id_, (void *)(intptr_t)sb->_fd);

    TRACE("EXIT\n");

    return NULL;
}

int SharedBuffer::retrieve_buffer_id_(const std::string &name)
{
    TRACE("ENTER: name=%s\n", name.c_str());

    int id = -1;

    IPC ipc;

    if (!ipc.client_setup(name))
    {
        TRACE("EXIT: connection failed\n");

        return -1;
    }

    bool success = false;

#if defined(ANDROID)
    success = ipc.getfd(id);
#else
    success = ipc.get(id);
#endif

    if (!success)
    {
        TRACE("EXIT: connection failed\n");

        return -1;
    }

    TRACE("EXIT: id=%d\n", id);

    return id;
}

void *SharedBuffer::shm_attach_(int id)
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

    return pshm;
}

#if defined(ANDROID)
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
            TRACE("EXIT: UNMAP_FAILED (%d) - %s\n", size, strerror(errno));

            return ret;
        }
    }

    TRACE("EXIT: %d\n", ret);

    return ret;
}

#endif // ANDROID
#endif // USE_SYSV_SHM || ANDROID
