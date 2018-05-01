AM_CFLAGS = \
	$(WARNING_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS)

AM_CPPFLAGS = \
	$(CZMQ_CFLAGS) -I${top_srcdir}/src/common/libutil

AM_LDFLAGS = \
	$(CODE_COVERAGE_LIBS)

noinst_LTLIBRARIES = libutil.la

libutil_la_SOURCES = \
	xzmalloc.c \
	xzmalloc.h \
	log.c \
	log.h \
	oom.h \
	shortjansson.h \
	nodeset.c \
	nodeset.h \
	veb.c \
	veb.h \
	monotime.c \
	monotime.h

EXTRA_DIST = veb_mach.c
