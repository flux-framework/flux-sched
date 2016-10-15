AM_CFLAGS = @GCCWARN@ $(CODE_COVERAGE_CFLAGS)
AM_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

AM_CPPFLAGS = -I$(top_srcdir) $(JANSSON_CFLAGS)

TESTS_ENVIRONMENT = \
    LUA_PATH="$(abs_top_srcdir)/rdl/?.lua;$(FLUX_PREFIX)/share/lua/5.1/?.lua;$(LUA_PATH);;" \
    LUA_CPATH="$(abs_top_builddir)/rdl/?.so;$(FLUX_PREFIX)/lib64/lua/5.1/?.so;$(LUA_CPATH);;" \
    TESTRDL_INPUT_FILE="$(abs_top_srcdir)/conf/hype.lua"

TESTS = trdl

check_PROGRAMS = $(TESTS)
trdl_SOURCES = trdl.c
trdl_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/rdl
trdl_LDADD = $(top_builddir)/rdl/libflux-rdl.la \
    $(top_builddir)/src/common/liblsd/liblsd.la \
    $(top_builddir)/src/common/libutil/libutil.la \
    $(FLUX_CORE_LIBS) $(LUA_LIB) $(JANSSON_LIBS) $(CZMQ_LIBS)

EXTRA_DIST= test-rdl.lua
