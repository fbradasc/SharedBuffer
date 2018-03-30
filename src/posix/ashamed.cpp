#include <sstream>
#include <iostream>
#include <map>
#include "TCPServer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */
#include <fcntl.h>    /* For O_* constants */
#include <syslog.h>

# include <linux/ashmem.h>

#define TRACE()  cout << __FILE__ << ":" << __FUNCTION__ << "," << __LINE__

#define ASHMEM_DEVICE   "/dev/ashmem"

TCPServer tcp;

typedef std::map<string, int> shared_buffers_t;

shared_buffers_t shared_buffers;

int ion_fd = -1;

int ashamed_open_(const string& name, int oflag, mode_t mode)
{
TRACE() << " - ENTER: " << name << "," << oflag << "," << mode << endl;

    bool created = false;

    int fd = -1;

    try
    {
        fd = shared_buffers.at(name);
    }
    catch (...)
    {
    }

    if ((oflag & O_CREAT) != 0)
    {
        // Create the shared memory object if it does not exist.
        //
        // The object's permission bits are set according to the low-order 9 bits of mode
        //
        // A  new  shared  memory object initially has zero length.
        //
        // The newly allocated bytes of a shared memory object are automatically initialized to 0.
        //
        if (fd >= 0)
        {
            if ((oflag & O_EXCL) != 0)
            {
                // If a shared memory object with the given name already exists, return an error.
                //
                // The check for the existence of the object, and its creation if it does not exist, are performed atomically.
                //

TRACE() << " - EXIT: O_EXCL" << endl;
                return -O_EXCL;
            }
        }
        else
        {
            fd = open(ASHMEM_DEVICE, O_RDWR);

            created = true;
        }
    }

    if (fd < 0)
    {
TRACE() << " - EXIT: " << fd << endl;
        return fd;
    }

    if ((oflag & O_TRUNC) != 0)
    {
        // If the shared memory object already exists, truncate it to zero bytes.
        //
        if (ioctl(fd, ASHMEM_SET_SIZE, 0) < 0)
        {
            if (created)
            {
                close(fd);
            }

TRACE() << " - EXIT: O_TRUNC" << endl;
            return -O_TRUNC;
        }
    }

#if 0
    if ((oflag & O_RDONLY) != 0)
    {
        if ((oflag & O_RDWR) != 0)
        {
            // Both read-only and read-write -> error

            if (created)
            {
                close(fd);
            }

            return -( O_RDWR | O_RDONLY );
        }

        // Open the object for read-only access.
        //
    }

    if ((oflag & O_RDWR) != 0)
    {
        // Open the object for read-write access.
        //
    }
#endif

    if (created)
    {
        shared_buffers[name] = fd;
    }

TRACE() << " - EXIT: " << fd << endl;
    return fd;
}

int ashamed_unlink_(const string& name)
{
TRACE() << " - ENTER: " << name << endl;
    try
    {
        int fd = shared_buffers.at(name);

        if (close(fd) >= 0)
        {
            shared_buffers.erase(name);

TRACE() << " - EXIT: 0" << endl;
            return 0;
        }
    }
    catch (...)
    {
    }

TRACE() << " - EXIT: -1" << endl;
    return -1;
}

int ashamed_truncate_(int fd, size_t size)
{
TRACE() << " - ENTER: " << fd << "," << size << endl;
    if (fd < 0)
    {
TRACE() << " - EXIT: " << fd << endl;
        return fd;
    }

    int ret = ioctl(fd, ASHMEM_SET_SIZE, size);

TRACE() << " - EXIT: " << ret << endl;
    return ret;
}

void * loop(void * m)
{
TRACE() << " - ENTER" << endl;
    pthread_detach(pthread_self());

    while (true)
    {
        srand(time(NULL));

        vector<string> vstr = tcp.getMessage(':');

        size_t pnum = vstr.size();

for (int i=0; i<pnum; i++) { TRACE() << " - vstr[" << i << "]: " << vstr[i] << endl; }

        int fd = -1;

        if ((pnum == 4) && (vstr[0] == "O")) // open
        {
            fd = ashamed_open_(vstr[1], atoi(vstr[2].c_str()), atoi(vstr[3].c_str()));

TRACE() << " - Send: " << fd << endl;
            tcp.send_fd(fd);
        }
        else
        if ((pnum == 2) && (vstr[0] == "U")) // unlink
        {
            fd = ashamed_unlink_(vstr[1]);

TRACE() << " - Send: " << fd << endl;
            tcp.send_rv(fd);
        }
        else
        if ((pnum == 2) && (vstr[0] == "T")) // unlink
        {
            fd = tcp.recv_fd();

            fd = ashamed_truncate_(fd, atoi(vstr[1].c_str()));

TRACE() << " - Send: " << fd << endl;
            tcp.send_rv(fd);
        }

        usleep(1000);
    }

    tcp.detach();
}

static void daemonize()
{
    pid_t pid;

    // Fork off the parent process
    //
    pid = fork();

    // An error occurred
    //
    if (pid < 0)
    {
        cout << "Failing to fork off the parent process" << endl;

        exit(EXIT_FAILURE);
    }

    // Success: Let the parent terminate
    //
    if (pid > 0)
    {
        exit(EXIT_SUCCESS);
    }

    // On success: The child process becomes session leader
    //
    if (setsid() < 0)
    {
        cout << "Failing to make the child process the session leader" << endl;

        exit(EXIT_FAILURE);
    }

    // Catch, ignore and handle signals
    //
    // TODO: Implement a working signal handler
    //
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    // Fork off for the second time
    //
    pid = fork();

    // An error occurred
    //
    if (pid < 0)
    {
        cout << "Failing to fork off the second time" << endl;

        exit(EXIT_FAILURE);
    }

    // Success: Let the parent terminated
    //
    if (pid > 0)
    {
        exit(EXIT_SUCCESS);
    }

    // Set new file permissions
    //
    umask(0);

    // Change the working directory to the root directory
    // or another appropriated directory
    //
    chdir("/");

    // Close all open file descriptors
    //
    for (int x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }

    // Open the log file
    //
    openlog ("ashamed", LOG_PID, LOG_DAEMON);
}

int main()
{
    // daemonize();

    ion_fd = oepn("/dev/ion", O_RDONLY);

    if (ion_fd > 0)
    {
        pthread_t msg;

        tcp.setup(11999);

        if (pthread_create(&msg, NULL, loop, (void *)0) == 0)
        {
            tcp.receive();
        }
    }
    else
    {
        // Error
    }

    // syslog(LOG_NOTICE, "ashamed daemon terminated.");

    // closelog();

    return(0);
}
