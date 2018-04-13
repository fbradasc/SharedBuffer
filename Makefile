DBG_FLAGS := \
             -DDEBUG_SHB \
             -DDEBUG_USERVER \
             -DDEBUG_UCLIENT \
             -DDEBUG_MAIN \
             -DDEBUG_ASHAMED

HDRS      := \
             -I$(PWD)/src \
             -I$(PWD)/inc \

LIB_SRC   := \
             src/ipc.cpp          \
             src/ion.cpp          \
             src/SharedBuffer.cpp \

SRCS      := \
             $(LIB_SRC) \
             src/crapchat.cpp

TARGET    ?= $(shell uname -m)

ifneq ($(findstring android, $(TARGET)),)
  include Makefile.android
else
  include Makefile.unix
endif

default:
	echo
	echo "make TARGET=<android|unix>"
	echo

bin/sysv/$(TARGET)/crapchat: bin/sysv/$(TARGET) $(SRCS)
	$(CXX) $(CXXFLAGS) -DUSE_SYSV_SHM $(LDFLAGS) $(SRCS) -o $@

bin/posix/$(TARGET)/crapchat: bin/posix/$(TARGET) $(SRCS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SRCS) -o $@

bin/sysv/$(TARGET):
	mkdir -p $@

bin/posix/$(TARGET):
	mkdir -p $@

clean:
	rm -rf bin
