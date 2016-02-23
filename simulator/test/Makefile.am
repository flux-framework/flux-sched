AM_CFLAGS = @GCCWARN@

AM_CPPFLAGS = $(JSON_CFLAGS) $(CZMQ_CFLAGS) \
    -I $(FLUX_BUILDDIR)

TESTS = tsched_lib

TESTS_ENVIRONMENT = \
    TESTSIM_INPUT_FILE="$(abs_top_srcdir)/conf/hype-io.lua" \
    LUA_PATH="$(abs_top_srcdir)/rdl/?.lua;$(FLUX_PREFIX)/share/lua/5.1/?.lua;$(LUA_PATH);;" \
    LUA_CPATH="$(abs_top_builddir)/rdl/.libs/?.so;$(FLUX_PREFIX)/lib64/lua/5.1/?.so;$(LUA_CPATH);;"

check_PROGRAMS = $(TESTS)
tsched_lib_SOURCES = tsched_lib.c $(top_srcdir)/simulator/scheduler.c
tsched_lib_CFLAGS = $(AM_CFLAGS) $(FLUX_CORE_CFLAGS) -I$(top_srcdir) \
    -I$(top_srcdir)/rdl
tsched_lib_LDADD = $(top_builddir)/foreign/libforeign.la \
    $(top_builddir)/rdl/libflux-rdl.la $(top_builddir)/simulator/libflux-sim.la \
    $(top_builddir)/src/common/libtap/libtap.la \
    $(FLUX_CORE_LIBS) $(JSON_LIBS) $(CZMQ_LIBS)
