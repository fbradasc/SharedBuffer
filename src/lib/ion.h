#ifndef ION_H
#define ION_H

#include <sys/types.h>
#include <linux/ion.h>

class ION
{
public:
    ION();

    int close   ();
    int alloc   (size_t len, size_t align, unsigned int heap_mask, unsigned int flags);
    int alloc_fd(size_t len, size_t align, unsigned int heap_mask, unsigned int flags, int &handle_fd);
    int free    ();
    int map     (size_t length, int prot, int flags, off_t offset, void* &ptr, int &map_fd);
    int share   (int &share_fd);
    int import  (int share_fd);
    int sync_fd (int handle_fd);

private:
    int _fd;
    int _handle;

    int _ioctl(int req, void *arg);
};

#endif // ION_H
