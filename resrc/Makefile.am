AM_CFLAGS = $(WARNING_CFLAGS) $(CODE_COVERAGE_CFLAGS)
AM_LDFLAGS = $(CODE_COVERAGE_LIBS)

AM_CPPFLAGS = -I$(top_srcdir) \
    $(JANSSON_CFLAGS) $(CZMQ_CFLAGS)

SUBDIRS = . test

noinst_LTLIBRARIES = libflux-resrc.la

noinst_HEADERS = resrc_api.h resrc_api_internal.h \
                 resrc.h resrc_tree.h resrc_flow.h resrc_reqst.h

libflux_resrc_la_SOURCES = resrc.c resrc_tree.c resrc_flow.c resrc_reqst.c
libflux_resrc_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/rdl $(IDSET_CFLAGS)
libflux_resrc_la_LIBADD = $(top_builddir)/rdl/libflux-rdl.la \
    $(top_builddir)/src/common/libutil/libutil.la \
    $(DL_LIBS) $(HWLOC_LIBS) $(UUID_LIBS) $(JANSSON_LIBS) $(CZMQ_LIBS) $(IDSET_LIBS)
libflux_resrc_la_LDFLAGS = $(AM_LDFLAGS) $(fluxlib_ldflags) \
    -Wl,--version-script=$(srcdir)/resrc_version.map

EXTRA_DIST = resrc_version.map
