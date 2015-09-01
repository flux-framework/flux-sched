include ../../Makefile.inc

CFLAGS = $(COMMON_CFLAGS) $(SCHEDCOMM_CFLAGS) $(FLUX_CFLAGS) $(RDL_CFLAGS) $(RESRC_CFLAGS)
LIBS = $(SCHEDCOMM_LIBS) $(FLUX_LIBS) $(RDL_LIBS) $(RESRC_LIBS)

TESTRESRC_INPUT_FILE := $(abs_topdir)/conf/hype.lua

LUA_PATH := $(LUA_PATH);$(abs_topdir)/rdl/?.lua;$(FLUX_SRCDIR)/src/bindings/lua/?.lua;;;
LUA_CPATH := $(LUA_CPATH);$(abs_topdir)/rdl/?.so;$(FLUX_SRCDIR)/src/bindings/lua/.libs/?.so;;;

export CFLAGS TESTRESRC_INPUT_FILE LUA_PATH LUA_CPATH

BUILD = tresrc

all: $(BUILD)

tresrc: tresrc.o
	$(CC) -o $@ $^ $(LIBS)

check: tresrc
	./tresrc

clean:
	rm -f *.o a.out
	rm -f $(BUILD)
