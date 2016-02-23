AM_CFLAGS = @GCCWARN@

AM_CPPFLAGS = -I$(top_srcdir) $(JSON_CFLAGS)

TESTS_ENVIRONMENT = \
    LUA_PATH="$(abs_top_srcdir)/rdl/?.lua;$(FLUX_PREFIX)/share/lua/5.1/?.lua;$(LUA_PATH);;" \
    LUA_CPATH="$(abs_top_builddir)/rdl/.libs/?.so;$(FLUX_PREFIX)/lib64/lua/5.1/?.so;$(LUA_CPATH);;" \
    TESTRDL_INPUT_FILE="$(abs_top_srcdir)/conf/hype.lua"

TESTS = trdl

check_PROGRAMS = $(TESTS)
trdl_SOURCES = trdl.c
trdl_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/rdl
trdl_LDADD = $(top_builddir)/rdl/libflux-rdl.la \
    $(LUA_LIB) $(JSON_LIBS) $(CZMQ_LIBS)

EXTRA_DIST= test-rdl.lua
