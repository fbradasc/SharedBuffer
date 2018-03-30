#!/bin/sh

###############################################################################
#                                                                             #
#                       SYSV API Implementation START                         #
#                                                                             #
###############################################################################

mkdir -p bin/sysv

H_FLAGS="-Isrc -Iinc"

SYSV_H_FLAGS="${H_FLAGS} -Iinc/sysv"

SYSV_CRAPCHAT_SRCS="src/UServer.cpp src/UClient.cpp src/ion.cpp src/sysv/SharedBuffer.cpp src/sysv/crapchat.cpp"

DEBUG_FLAGS="-DDEBUG_SHB -DDEBUG_USERVER -DDEBUG_UCLIENT -DDEBUG_MAIN"
# DEBUG_FLAGS=""

g++ -g -m32 ${SYSV_CRAPCHAT_SRCS} ${DEBUG_FLAGS} ${SYSV_H_FLAGS} -o bin/sysv/crapchat_x86_64 -lrt -lpthread

./setCrossEnvironment-armeabi-v7a.sh sh -c \
"\$CXX \$CXXFLAGS -D_LINUX_IPC_H -D__ANDROID__ ${DEBUG_FLAGS} ${SYSV_H_FLAGS} -pie \$LDFLAGS ${SYSV_CRAPCHAT_SRCS} -o bin/sysv/crapchat_android"

###############################################################################
#                                                                             #
#                       SYSV API Implementation END                           #
#                                                                             #
###############################################################################

exit

###############################################################################
#                                                                             #
#                     POSIX API Implementation START                          #
#                                                                             #
#                            WORK IN PROGRESS                                 #
#                                                                             #
###############################################################################

mkdir -p bin/posix

POSIX_H_FLAGS="${H_FLAGS} -Iinc/posix"

POSIX_CRAPCHAT_SRCS="src/posix/SharedBuffer.cpp src/posix/crapchat.cpp"

g++ -g -m32 ${POSIX_CRAPCHAT_SRCS} -o bin/posix/crapchat_x86_64 -lrt

POSIX_CRAPCHAT_SRCS="src/UClient.cpp src/posix/posix_shm.cpp ${POSIX_CRAPCHAT_SRCS}"

./setCrossEnvironment-armeabi-v7a.sh sh -c \
"\$CXX \$CXXFLAGS -D_LINUX_IPC_H -D__ANDROID__ -pie \$LDFLAGS ${POSIX_H_FLAGS} ${POSIX_CRAPCHAT_SRCS} -o bin/posix/crapchat_android"

ASHAMED_SRCS="src/UServer.cpp src/posix/ashamed.cpp"

./setCrossEnvironment-armeabi-v7a.sh sh -c \
"\$CXX \$CXXFLAGS -D_LINUX_IPC_H -D__ANDROID__ -pie \$LDFLAGS ${POSIX_H_FLAGS} ${ASHAMED_SRCS} -o bin/posix/ashamed"

###############################################################################
#                                                                             #
#                     POSIX API Implementation END                            #
#                                                                             #
###############################################################################
