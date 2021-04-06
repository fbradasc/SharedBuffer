#ifndef SHAREDBUFFER_H
#define SHAREDBUFFER_H

#include <string>
#include <stdlib.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/mman.h>

#if defined(USE_SYSV_SHM) || defined(ANDROID)
#include <sys/shm.h>
#include "ion.h"
#endif

class IPC;

class SharedBuffer
{
public:
    SharedBuffer();

    ~SharedBuffer();

    void unmap();

    bool map(const char* name, size_t size=0, bool exclusive=false);

    bool grab();

    bool release();

    inline void* data()
    {
        return( (NULL != _pshm) ? &_pshm->_buffer : NULL );
    }

    inline size_t size()
    {
        return( (NULL != _pshm) ? _pshm->_size : 0 );
    }

    inline const char* name()
    {
        return(_name);
    }

    inline bool grabbed()
    {
        return(_grabbed);
    }

    inline bool owner()
    {
        return(_owner);
    }

    inline bool exclusive()
    {
        return((NULL != _pshm) ? _pshm->_exclusive : false);
    }

    inline bool locked(unsigned int who)
    {
        return(((NULL != _pshm) && (who < USERS)) ? _pshm->_locks[who] : false);
    }

    // timeout_ms:
    // -1 : wait forever until notified
    //  0 : try once -> return true if notified, false if not notified
    // >0 : return true if notified within timeout_ms, false if timeout_ms is elaphsed
    //
    bool wait(int timeout_ms=-1);

    bool notify();

    void dump();

    static bool destroy(const char *name);

private:
    enum
    {
        OWNER = 0,
        OTHER,
        THIRD,
        USERS
    };

    class SharedBufferPrivate
    {
    private:
        friend class SharedBuffer;

        bool   _exclusive;
        bool   _locks[USERS];   // 0: owner access, 1: other access, 2: exclusive client attach
        size_t _size;
        char   _buffer;
    };

    int                  _fd;
    char*                _name;
    SharedBufferPrivate* _pshm;  // Shared memory buffer address.
    bool                 _owner;
    bool                 _grabbed;

    pthread_t            _owner_thread;

    bool map_(const char* name, size_t size=0, bool exclusive=false);

    SharedBufferPrivate *attach_(const char *name);

    SharedBufferPrivate *create_(const char *name, size_t size, int & fd);

    bool grab_(bool grab);

#if defined(USE_SYSV_SHM) || defined(ANDROID)

    static void *publisher_(void * m);

    static void publish_buffer_id_(IPC *conn, void *data);

    int retrieve_buffer_id_(const std::string &name);

    void *shm_attach_(int id);

#if defined(ANDROID)
    ION _ion;

    int shmctl(int __shmid, int __cmd, struct shmid_ds *__buf);

    int shmget(key_t __key, size_t __size, int __shmflg);

    void *shmat(int __shmid, const void *__shmaddr, int __shmflg);

    int shmdt(const void *__shmaddr);
#endif // ANDROID
#endif // USE_SYSV_SHM || ANDROID
};

#endif // SHAREDBUFFER_H
