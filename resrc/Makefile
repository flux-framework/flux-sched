include ../Makefile.inc

SUBDIRS = test
CFLAGS = $(COMMON_CFLAGS) $(FLUX_CFLAGS) $(RDL_CFLAGS)
LIBS = $(FLUX_LIBS) $(RDL_LIBS)
TESTRESRC_INPUT_FILE := $(abs_topdir)/conf/hype.lua
LUA_PATH = $(abs_topdir)/rdl/?.lua;$(FLUX_SRCDIR)/src/bindings/lua/?.lua;;;
LUA_CPATH = $(abs_topdir)/rdl/?.so;$(FLUX_SRCDIR)/src/bindings/lua/.libs/?.so;;;

export TESTRESRC_INPUT_FILE LUA_PATH LUA_CPATH

BUILD = libresrc.so.0

LDADD_CLI = $(FLUX_SRCDIR)/src/common/libutil/.libs/libutil.a \
            $(FLUX_SRCDIR)/src/common/libtap/.libs/libtap.a

test_cppflags = \
	-DTEST_MAIN \
	-I$(FLUX_SRCDIR)/src/common/libtap

test_resrc_t_SOURCES = resrc.c

all: $(BUILD)
	for subdir in $(SUBDIRS); do make -C $$subdir $@; done

libresrc.so.0: resrc.o resrc_tree.o
	$(CC) -shared -Wl,--version-script=resrc_version.map -o $@ $^ $(LIBS)
	ln -sf $(shell basename $@) $(@:.0=)

test_resrc_t.o: $(test_resrc_t_SOURCES)
	$(CC) $(test_cppflags) $(CFLAGS) -c -o $@ $^

test_resrc.t: test_resrc_t.o resrc_tree.o
	$(CC) -o $@ $^ $(LDADD_CLI) $(LIBS)

check: test_resrc.t
	./test_resrc.t

clean:
	rm -f *.so *.o $(BUILD)
	for subdir in $(SUBDIRS); do make -C $$subdir $@; done
