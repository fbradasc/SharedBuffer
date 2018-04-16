                                     SharedBuffer
                                     ------------

Sample code to demostrate the use of shared memory between different processes (not forked)

Supported targets:

- linux / unix

  By means of the following APIs:

  - SYSV  (shmctl, shmget, shmat, shmdt) 
  - POSIX (ftruncate, shm_unlink, shm_open)

- Android

  By means of the following APIs:

  - SYSV (partially implemented using the ION library)

Unix sockets and sendmsg/recvmsg (ancillary data) are used for the IPC.
