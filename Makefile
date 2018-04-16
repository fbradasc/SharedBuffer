DBG_FLAGS :=

HDRS      := \
             -I$(PWD)/src/lib \
             -I$(PWD)/inc/lib \

LIB_SRC   := \
             src/lib/ipc.cpp          \
             src/lib/ion.cpp          \
             src/lib/SharedBuffer.cpp \

SRCS      := \
             $(LIB_SRC) \
             src/test/crapchat.cpp

TARGET    ?= $(shell uname -m)

DEBUG     ?= off

help:
	@echo
	@echo -e "make TARGET=android [DEBUG=<on|off>] [RUN_ARGS=...] <all|install|run>"
	@echo
	@echo -e "\tBuild for android"
	@echo
	@echo -e "make TARGET=linux [RUN_ARGS=...] <all|run_sysv|run_posix>"
	@echo
	@echo -e "\tBuild for linux"
	@echo
	@echo -e "make clean"
	@echo
	@echo -e "\tBuild cleanup"
	@echo
	@echo -e "make help"
	@echo
	@echo -e "\tShows this help"
	@echo

ifeq ($(DEBUG),on)
DBG_FLAGS := \
             -DDEBUG_SHB \
             -DDEBUG_USERVER \
             -DDEBUG_UCLIENT \
             -DDEBUG_MAIN \
             -DDEBUG_ASHAMED
endif

ifneq ($(findstring android, $(TARGET)),)
  include Makefile.android
else
  include Makefile.unix
endif

bin/sysv/$(TARGET)/crapchat: bin/sysv/$(TARGET) $(SRCS)
	@$(CXX) $(CXXFLAGS) -DUSE_SYSV_SHM $(LDFLAGS) $(SRCS) -o $@

bin/posix/$(TARGET)/crapchat: bin/posix/$(TARGET) $(SRCS)
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SRCS) -o $@

bin/sysv/$(TARGET):
	@mkdir -p $@

bin/posix/$(TARGET):
	@mkdir -p $@

clean:
	@rm -rf bin
