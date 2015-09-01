include ../../Makefile.inc

CFLAGS = $(COMMON_CFLAGS) $(SCHEDCOMM_CFLAGS) $(FLUX_CFLAGS) $(RDL_CFLAGS)
LIBS = $(SCHEDCOMM_LIBS) $(FLUX_LIBS) $(RDL_LIBS)

TESTRDL_INPUT_FILE := $(abs_topdir)/conf/hype.lua

LUA_PATH = $(abs_topdir)/rdl/?.lua;$(FLUX_SRCDIR)/src/bindings/lua/?.lua;;;
LUA_CPATH = $(abs_topdir)/rdl/?.so;$(FLUX_SRCDIR)/src/bindings/lua/.libs/?.so;;;

export CFLAGS TESTRDL_INPUT_FILE LUA_PATH LUA_CPATH

BUILD = trdl

all: $(BUILD)

trdl: trdl.o
	$(CC) -o $@ $^ $(LIBS)

check:
	./trdl

clean:
	rm -f *.o a.out
	rm -f $(BUILD)
