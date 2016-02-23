AM_CFLAGS = @GCCWARN@

AM_CPPFLAGS = $(JSON_CFLAGS) $(CZMQ_CFLAGS) $(XML2_CFLAGS) \
    -I $(FLUX_BUILDDIR)

SUBDIRS = . test

lib_LTLIBRARIES = libflux-resrc.la

fluxschedinclude_HEADERS = resrc.h resrc_tree.h resrc_reqst.h

libflux_resrc_la_SOURCES = resrc.c resrc_tree.c resrc_reqst.c
libflux_resrc_la_CFLAGS = -I$(top_srcdir)/rdl
libflux_resrc_la_LIBADD = $(top_builddir)/rdl/libflux-rdl.la \
    $(JSON_LIBS) $(CZMQ_LIBS) $(XML2_LIBS)
libflux_resrc_la_LDFLAGS = $(fluxlib_ldflags)
