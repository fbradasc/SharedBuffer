                                     SharedBuffer
                                     ------------

Sample code to demostrate the use of shared memory between different processes (not forked)

Unix sockets and sendmsg/recvmsg (ancillary data) are used to share the memory descriptors

Used APIs for memory management:

- SYSV  (shmctl, shmget, shmat, shmdt) 
- Posix (ftruncate, shm_unlink, shm_open) [Work in progress]

Supperted targets:

- linux
- android, by mean of ION APIs
