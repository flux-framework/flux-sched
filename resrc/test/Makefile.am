AM_CFLAGS = @GCCWARN@ $(CODE_COVERAGE_CFLAGS)
AM_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

AM_CPPFLAGS = -I$(top_srcdir) $(JSON_CFLAGS) $(CZMQ_CFLAGS) $(FLUX_CORE_CFLAGS)

TESTS_ENVIRONMENT = \
    TESTRESRC_INPUT_FILE="$(abs_top_srcdir)/conf/hype.lua" \
    LUA_PATH="$(abs_top_srcdir)/rdl/?.lua;$(FLUX_PREFIX)/share/lua/5.1/?.lua;$(LUA_PATH);;" \
    LUA_CPATH="$(abs_top_builddir)/rdl/?.so;$(FLUX_PREFIX)/lib64/lua/5.1/?.so;$(LUA_CPATH);;"

TESTS = tresrc

check_PROGRAMS = $(TESTS)
tresrc_SOURCES = tresrc.c
tresrc_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/resrc
tresrc_LDADD = $(top_builddir)/resrc/libflux-resrc.la \
    $(top_builddir)/src/common/liblsd/liblsd.la \
    $(top_builddir)/src/common/libutil/libutil.la \
    $(top_builddir)/src/common/libtap/libtap.la \
    $(LUA_LIB) $(JSON_LIBS) $(CZMQ_LIBS)
