#!/bin/sh

DEBUG_FLAGS="-DDEBUG_SHB -DDEBUG_USERVER -DDEBUG_UCLIENT -DDEBUG_MAIN -DDEBUG_ASHAMED"

CRAPCHAT_SRCS="src/ipc.cpp src/ion.cpp src/SharedBuffer.cpp src/crapchat.cpp"

H_FLAGS="-Isrc -Iinc"

mkdir -p bin/sysv

./setCrossEnvironment-armeabi-v7a.sh sh -c \
"\$CXX \$CXXFLAGS -D_LINUX_IPC_H -DUSE_SYSV_SHM ${DEBUG_FLAGS} ${H_FLAGS} -pie \$LDFLAGS ${CRAPCHAT_SRCS} -o bin/sysv/crapchat_android"

g++ -g -fpermissive -Wint-to-pointer-cast -DUSE_SYSV_SHM ${DEBUG_FLAGS} ${H_FLAGS} ${CRAPCHAT_SRCS} -o bin/sysv/crapchat_x86_64 -lrt -lpthread

mkdir -p bin/posix

g++ -g -fpermissive -Wint-to-pointer-cast ${DEBUG_FLAGS} ${H_FLAGS} ${CRAPCHAT_SRCS} -o bin/posix/crapchat_x86_64 -lrt -lpthread
