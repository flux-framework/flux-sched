AM_CFLAGS = @GCCWARN@

AM_CPPFLAGS = $(JSON_CFLAGS) \
    -I $(FLUX_BUILDDIR)

noinst_LTLIBRARIES = libforeign.la

libforeign_la_SOURCES = \
    $(FLUX_BUILDDIR)/src/common/libutil/base64.c \
    $(FLUX_BUILDDIR)/src/common/libutil/jsonutil.c \
    $(FLUX_BUILDDIR)/src/common/libutil/log.c \
    $(FLUX_BUILDDIR)/src/common/libutil/xzmalloc.c

TESTS_ENVIRONMENT = \
    LUA_PATH="$(abs_top_srcdir)/rdl/?.lua;$(FLUX_PREFIX)/share/lua/5.1/?.lua;$(LUA_PATH);;" \
    LUA_CPATH="$(abs_top_builddir)/rdl/.libs/?.so;$(FLUX_PREFIX)/lib64/lua/5.1/?.so;$(LUA_CPATH);;" \
    TESTRDL_INPUT_FILE="$(abs_top_srcdir)/conf/hype.lua"

TESTS = trdl

check_PROGRAMS = $(TESTS)
trdl_SOURCES = trdl.c
trdl_CFLAGS = $(AM_CFLAGS) -I..
trdl_LDADD = ../librdl.la libforeign.la $(LUA_LIB) $(JSON_LIBS) $(CZMQ_LIBS)

EXTRA_DIST= test-rdl.lua
