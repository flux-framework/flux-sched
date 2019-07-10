AM_CFLAGS = \
    $(WARNING_CFLAGS) \
    $(CODE_COVERAGE_CFLAGS)

AM_LDFLAGS = \
    $(CODE_COVERAGE_LDFLAGS)

AM_CPPFLAGS = \
    $(FLUX_CORE_CFLAGS) \
    $(ZMQ_CFLAGS)

noinst_LTLIBRARIES = \
    libschedutil.la

libschedutil_la_SOURCES = \
    schedutil.h \
    hello.h \
    hello.c \
    ready.h \
    ready.c \
    ops.h \
    ops.c \
    alloc.h \
    alloc.c \
    free.h \
    free.c
