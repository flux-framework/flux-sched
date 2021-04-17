AM_CFLAGS = \
	$(WARNING_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS)

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	$(CZMQ_CFLAGS)

AM_LDFLAGS = \
	$(CODE_COVERAGE_LDFLAGS)

SUBDIRS = . test

noinst_LTLIBRARIES = libplanner.la
noinst_HEADERS = planner.h planner_multi.h

libplanner_la_SOURCES = planner.c planner_multi.c
libplanner_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/resource/planner
libplanner_la_LIBADD = \
	$(top_builddir)/src/common/libutil/libutil.la \
	$(top_builddir)/src/common/librbtree/librbtree.la \
	$(top_builddir)/src/common/libczmqcontainers/libczmqcontainers.la \
	$(CZMQ_LIBS)
