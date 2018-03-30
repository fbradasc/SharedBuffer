#ifndef POSIX_SHM
#define POSIX_SHM

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */
#include <fcntl.h>    /* For O_* constants */

#if defined(__ANDROID__)
extern int posix_shm_truncate(int fd, size_t size);
extern int posix_shm_unlink  (const char* name);
extern int posix_shm_open    (const char *name, int oflag, mode_t mode);
#else
# define posix_shm_truncate ftruncate
# define posix_shm_unlink   shm_unlink
# define posix_shm_open     shm_open
#endif

#endif // POSIX_SHM
