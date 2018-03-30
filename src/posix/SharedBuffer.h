#ifndef SHAREDBUFFER_H
#define SHAREDBUFFER_H

#include <stdlib.h>

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

    bool wait();

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

    char*                _name;
    SharedBufferPrivate* _pshm;  // Shared memory buffer address.
    bool                 _owner;
    bool                 _grabbed;

    bool map_(const char* name, size_t size=0, bool exclusive=false);

    bool grab_(bool grab);
};

#endif // SHAREDBUFFER_H
