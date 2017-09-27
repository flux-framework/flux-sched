AM_CFLAGS = $(WARNING_CFLAGS) $(CODE_COVERAGE_CFLAGS)
AM_LDFLAGS = $(CODE_COVERAGE_LIBS)

AM_CPPFLAGS = -I$(top_srcdir) $(JANSSON_CFLAGS) $(LUA_INCLUDE)

TESTS_ENVIRONMENT = \
    LUA_PATH="$(abs_top_srcdir)/rdl/?.lua;$(FLUX_PREFIX)/share/lua/5.1/?.lua;$(FLUX_PREFIX)/share/lua/5.1/fluxometer/?.lua;$(LUA_PATH);;" \
    LUA_CPATH="$(abs_top_builddir)/rdl/?.so;$(abs_top_builddir)/rdl/test/.libs/?.so;$(FLUX_PREFIX)/lib64/lua/5.1/?.so;$(LUA_CPATH);;" \
    TESTRDL_INPUT_FILE="$(abs_top_srcdir)/conf/hype.lua"

TESTS = trdl test-jansson.lua

check_PROGRAMS = trdl
check_LTLIBRARIES = janssontest.la
trdl_SOURCES = trdl.c
trdl_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/rdl
trdl_LDADD = $(top_builddir)/rdl/libflux-rdl.la \
    $(top_builddir)/src/common/liblsd/liblsd.la \
    $(top_builddir)/src/common/libutil/libutil.la \
    $(FLUX_CORE_LIBS) $(LUA_LIB) $(JANSSON_LIBS) $(CZMQ_LIBS)

janssontest_la_LDFLAGS = \
    -avoid-version -module -shared --disable-static -rpath /dev/null
janssontest_la_LIBADD = \
    $(top_builddir)/rdl/libjansson-lua.la \
    $(JANSSON_LIBS) $(LUA_LIB) $(FLUX_CORE_LIBS)
janssontest_la_SOURCES = \
    jansson-test.c

EXTRA_DIST = test-rdl.lua test-jansson.lua
