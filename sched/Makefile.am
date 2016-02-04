AM_CFLAGS = @GCCWARN@

AM_CPPFLAGS = $(JSON_CFLAGS) $(CZMQ_CFLAGS) $(FLUX_CORE_CFLAGS) \
    -I $(FLUX_BUILDDIR)

CONF = ../conf/hype.lua
FLUX_MODULE_PATH = $(top_srcdir)/sched
LUA_PATH = $(abs_top_srcdir)/rdl/?.lua;$(FLUX_PREFIX)/share/lua/5.1/?.lua;$${LUA_PATH};;
LUA_CPATH = $(abs_top_builddir)/rdl/.libs/?.so;$${LUA_CPATH};;

export FLUX_MODULE_PATH LUA_CPATH LUA_PATH

lib_LTLIBRARIES = schedsrv.la \
                  schedplugin1.la \
                  backfillplugin1.la
noinst_LTLIBRARIES = libforeign.la

include_HEADERS = schedsrv.h

libforeign_la_SOURCES = \
    $(FLUX_BUILDDIR)/src/common/libutil/log.c \
    $(FLUX_BUILDDIR)/src/common/libutil/xzmalloc.c

schedsrv_la_SOURCES = schedsrv.c rs2rank.c  rsreader.c
schedsrv_la_CFLAGS = $(AM_CFLAGS) -I../resrc
schedsrv_la_LIBADD = libforeign.la ../resrc/libresrc.la $(FLUX_CORE_LIBS) \
    ../simulator/libsim.la
schedsrv_la_LDFLAGS = -module -version-info 0:0:0

schedplugin1_la_SOURCES = schedplugin1.c
schedplugin1_la_CFLAGS = $(AM_CFLAGS) -I../resrc
schedplugin1_la_LIBADD = ../resrc/libresrc.la
schedplugin1_la_LDFLAGS = -module -version-info 0:0:0

backfillplugin1_la_SOURCES = backfillplugin1.c
backfillplugin1_la_CFLAGS = $(AM_CFLAGS) -I../resrc
backfillplugin1_la_LIBADD = ../resrc/libresrc.la
backfillplugin1_la_LDFLAGS = -module -version-info 0:0:0

bin_PROGRAMS = flux-waitjob
flux_waitjob_SOURCES = flux-waitjob.c
flux_waitjob_CFLAGS = $(AM_CFLAGS)
flux_waitjob_LDADD = $(JSON_LIBS) $(FLUX_CORE_LIBS) libforeign.la $(CZMQ_LIBS)
