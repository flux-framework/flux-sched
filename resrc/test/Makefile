include ../../Makefile.inc

CFLAGS = $(COMMON_CFLAGS) $(FLUX_CFLAGS) $(RDL_CFLAGS) $(RESRC_CFLAGS)
LIBS = $(FLUX_LIBS) $(RDL_LIBS) $(RESRC_LIBS) -lhwloc

TESTRESRC_INPUT_FILE := $(abs_topdir)/conf/hype.lua

LUA_PATH := $(LUA_PATH);$(abs_topdir)/rdl/?.lua;$(FLUX_SRCDIR)/src/bindings/lua/?.lua;;;
LUA_CPATH := $(LUA_CPATH);$(abs_topdir)/rdl/?.so;$(FLUX_SRCDIR)/src/bindings/lua/?.so;;;

export CFLAGS LUA_PATH LUA_CPATH

LDADD_CLI = $(FLUX_SRCDIR)/src/common/libutil/.libs/libutil.a \
            $(FLUX_SRCDIR)/src/common/liblsd/.libs/liblsd.a \
            $(FLUX_SRCDIR)/src/common/libtap/.libs/libtap.a

test_cppflags = \
	-I$(FLUX_SRCDIR)/src/common/libtap

BUILD = tresrc

all: $(BUILD)

tresrc: tresrc.o
	$(CC) $(test_cppflags) -o $@ $^ $(LIBS) $(LDADD_CLI)

check: tresrc
	./tresrc $(TESTRESRC_INPUT_FILE)
	./tresrc

clean:
	rm -f *.o a.out
	rm -f $(BUILD)
