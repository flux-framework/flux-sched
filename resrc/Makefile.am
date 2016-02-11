AM_CFLAGS = @GCCWARN@

AM_CPPFLAGS = $(JSON_CFLAGS) $(CZMQ_CFLAGS) \
    -I $(FLUX_BUILDDIR)

SUBDIRS = . test

libforeign_la_SOURCES = \
    $(FLUX_BUILDDIR)/src/common/libutil/log.c \
    $(FLUX_BUILDDIR)/src/common/libutil/xzmalloc.c

lib_LTLIBRARIES = libresrc.la
noinst_LTLIBRARIES = libforeign.la

include_HEADERS = resrc.h resrc_tree.h resrc_reqst.h

libresrc_la_SOURCES = resrc.c resrc_tree.c resrc_reqst.c
libresrc_la_CFLAGS = -I../rdl -I/usr/include/libxml2
libresrc_la_LIBADD = ../rdl/librdl.la libforeign.la
libresrc_la_LDFLAGS = -version-info 0:0:0
