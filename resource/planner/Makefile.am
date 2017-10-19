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
noinst_HEADERS = planner.h

libplanner_la_SOURCES = planner.c
libplanner_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/resource/planner
libplanner_la_LIBADD = \
	$(top_builddir)/src/common/libutil/libutil.la \
	$(CZMQ_LIBS)
