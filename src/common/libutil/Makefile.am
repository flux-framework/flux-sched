AM_CFLAGS = \
	@GCCWARN@ \
	$(CODE_COVERAGE_CFLAGS)

AM_CPPFLAGS = \
	$(CZMQ_CFLAGS)

AM_LDFLAGS = \
	$(CODE_COVERAGE_LDFLAGS)

noinst_LTLIBRARIES = libutil.la

libutil_la_SOURCES = \
	xzmalloc.c \
	xzmalloc.h \
	log.c \
	log.h \
	oom.h \
	shortjson.h
