AM_CFLAGS = \
	$(WARNING_CFLAGS) \
	-Wno-parentheses \
	$(CODE_COVERAGE_CFLAGS)

AM_LDFLAGS = \
	$(CODE_COVERAGE_LIBS)

AM_CPPFLAGS = -I$(top_srcdir)

noinst_LTLIBRARIES = liblsd.la

liblsd_la_SOURCES = \
	list.c \
	list.h
