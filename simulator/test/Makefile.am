AM_CFLAGS = @GCCWARN@

AM_CPPFLAGS = $(JSON_CFLAGS) $(CZMQ_CFLAGS) \
    -I $(FLUX_BUILDDIR)

noinst_LTLIBRARIES = libforeign.la

libforeign_la_SOURCES = $(FLUX_BUILDDIR)/src/common/libtap/tap.c

TESTS = tsched_lib

TESTS_ENVIRONMENT = \
    TESTSIM_INPUT_FILE="$(abs_top_srcdir)/conf/hype-io.lua" \
    LUA_PATH="$(abs_top_srcdir)/rdl/?.lua;$(FLUX_PREFIX)/share/lua/5.1/?.lua;$(LUA_PATH);;" \
    LUA_CPATH="$(abs_top_builddir)/rdl/.libs/?.so;$(FLUX_PREFIX)/lib64/lua/5.1/?.so;$(LUA_CPATH);;"

check_PROGRAMS = $(TESTS)
tsched_lib_SOURCES = tsched_lib.c ../scheduler.c
tsched_lib_CFLAGS = $(AM_CFLAGS) $(FLUX_CORE_CFLAGS) -I.. -I../../rdl
tsched_lib_LDADD = libforeign.la ../../rdl/librdl.la ../libsim.la \
    $(FLUX_CORE_LIBS) $(JSON_LIBS) $(CZMQ_LIBS)
