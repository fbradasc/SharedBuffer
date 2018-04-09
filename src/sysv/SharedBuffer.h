#ifndef SHAREDBUFFER_H
#define SHAREDBUFFER_H

#include <stdlib.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/shm.h>

#if defined(__ANDROID__)
#include "ion.h"
#endif

class SharedBuffer
{
public:
    SharedBuffer();

    ~SharedBuffer();

    void unmap();

    bool map(int id, size_t size = 0, bool exclusive = false);

    bool grab();

    bool release();

    inline void* data()
    {
        return((NULL != _pshm) ? &_pshm->_buffer : NULL);
    }

    inline size_t size()
    {
        return((NULL != _pshm) ? _pshm->_size : 0);
    }

    inline int id()
    {
        return(_id);
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

    bool wait();

    bool notify();

    void dump();

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

        bool   _exclusive   ;
        bool   _locks[USERS];     // 0: owner access, 1: other access, 2: exclusive client attach
        size_t _size        ;
        char   _buffer      ;
    };

    int                  _id     ;
    SharedBufferPrivate* _pshm   ;  // Shared memory buffer address.
    bool                 _owner  ;
    bool                 _grabbed;

    bool map_(int id, size_t size, bool exclusive);

    SharedBufferPrivate *attach_(int id);

    SharedBufferPrivate *create_(int & id, size_t size);

    bool grab_(bool grab);

#if defined(__ANDROID__)
    ION _ion;

    int shmctl(int __shmid, int __cmd, struct shmid_ds *__buf);

    int shmget(key_t __key, size_t __size, int __shmflg);

    void *shmat(int __shmid, const void *__shmaddr, int __shmflg);

    int shmdt(const void *__shmaddr);
#endif
};

#endif // SHAREDBUFFER_H
