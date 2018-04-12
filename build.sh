#!/bin/sh

DEBUG_FLAGS="-DDEBUG_SHB -DDEBUG_USERVER -DDEBUG_UCLIENT -DDEBUG_MAIN -DDEBUG_ASHAMED"

CRAPCHAT_SRCS="src/UServer.cpp src/ion.cpp src/posix/SharedBuffer.cpp src/posix/crapchat.cpp"

H_FLAGS="-Isrc -Iinc"

mkdir -p bin/sysv

SYSV_H_FLAGS="-DUSE_SYSV_SHM ${H_FLAGS} -Iinc/sysv"

./setCrossEnvironment-armeabi-v7a.sh sh -c \
"\$CXX \$CXXFLAGS -D_LINUX_IPC_H -D__ANDROID__ ${DEBUG_FLAGS} ${SYSV_H_FLAGS} -pie \$LDFLAGS ${CRAPCHAT_SRCS} -o bin/sysv/crapchat_android"

g++ -g -fpermissive -Wint-to-pointer-cast ${CRAPCHAT_SRCS} ${DEBUG_FLAGS} ${SYSV_H_FLAGS} -o bin/sysv/crapchat_x86_64 -lrt -lpthread

mkdir -p bin/posix

POSIX_H_FLAGS="${H_FLAGS} -Iinc/posix"

g++ -g -fpermissive -Wint-to-pointer-cast ${CRAPCHAT_SRCS} ${DEBUG_FLAGS} ${POSIX_H_FLAGS} -o bin/posix/crapchat_x86_64 -lrt -lpthread
