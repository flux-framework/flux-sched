include ../../Makefile.inc

CFLAGS = $(COMMON_CFLAGS) $(SCHEDCOMM_CFLAGS) $(FLUX_CFLAGS) $(RDL_CFLAGS)
LIBS = $(SCHEDCOMM_LIBS) $(FLUX_LIBS) $(RDL_LIBS) $(KVS_LIBS)

TESTSIM_INPUT_FILE := $(abs_topdir)/conf/hype-io.lua

COMMON_OBJS = ../simulator.o
SCHEDULER_OBJS = ../scheduler.o

BUILD = tsched_lib

all: $(BUILD)

tsched_lib: tsched_lib.c $(COMMON_OBJS) $(SCHEDULER_OBJS)
	$(CC)  $(CFLAGS) $(test_cppflags) -o $@ $^ $(LIBS)

check: tsched_lib
	@export LUA_PATH="$(LUA_PATH);$(abs_topdir)/rdl/?.lua;$(FLUX_SRCDIR)/src/bindings/lua/?.lua;;;"; \
	export LUA_CPATH="$(LUA_CPATH);$(abs_topdir)/rdl/?.so;$(FLUX_BUILDDIR)/src/bindings/lua/.libs/?.so;;;"; \
	export TESTSIM_INPUT_FILE="$(TESTSIM_INPUT_FILE)"; \
	./tsched_lib

clean:
	rm -f *.o a.out
	rm -f $(BUILD)
