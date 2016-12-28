AM_CFLAGS = @GCCWARN@ $(CODE_COVERAGE_CFLAGS)
AM_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

AM_CPPFLAGS = -I$(top_srcdir) \
    $(JANSSON_CFLAGS) $(CZMQ_CFLAGS)

SUBDIRS = . test

noinst_LTLIBRARIES = libflux-resrc.la

noinst_HEADERS = resrc.h resrc_tree.h resrc_flow.h resrc_reqst.h planner.h

libflux_resrc_la_SOURCES = resrc.c resrc_tree.c resrc_flow.c resrc_reqst.c planner.c
libflux_resrc_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/rdl
libflux_resrc_la_LIBADD = $(top_builddir)/rdl/libflux-rdl.la \
    $(top_builddir)/src/common/libutil/libutil.la \
    $(JANSSON_LIBS) $(CZMQ_LIBS)
libflux_resrc_la_LDFLAGS = $(AM_LDFLAGS) $(fluxlib_ldflags) \
    -Wl,--version-script=$(srcdir)/resrc_version.map

EXTRA_DIST = resrc_version.map
