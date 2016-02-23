AM_CFLAGS = @GCCWARN@

AM_CPPFLAGS = -I$(top_srcdir) \
    $(JSON_CFLAGS) $(CZMQ_CFLAGS) $(FLUX_CORE_CFLAGS)

CONF = $(abs_top_srcdir)/conf/hype.lua
FLUX_MODULE_PATH = $(top_srcdir)/sched
LUA_PATH = $(abs_top_srcdir)/rdl/?.lua;$(FLUX_PREFIX)/share/lua/5.1/?.lua;$${LUA_PATH};;
LUA_CPATH = $(abs_top_builddir)/rdl/.libs/?.so;$${LUA_CPATH};;

fluxmod_LTLIBRARIES = schedsrv.la

schedplugin_LTLIBRARIES = schedplugin1.la \
                  backfillplugin1.la

fluxschedinclude_HEADERS = schedsrv.h rs2rank.h rsreader.h

schedsrv_la_SOURCES = schedsrv.c rs2rank.c rsreader.c
schedsrv_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/resrc
schedsrv_la_LIBADD = $(top_builddir)/resrc/libflux-resrc.la \
    $(FLUX_CORE_LIBS) $(JSON_LIBS) $(CZMQ_LIBS) \
    $(top_builddir)/src/common/libutil/libutil.la \
    $(top_builddir)/simulator/libflux-sim.la
schedsrv_la_LDFLAGS = $(fluxmod_ldflags) -module

schedplugin1_la_SOURCES = schedplugin1.c
schedplugin1_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/resrc
schedplugin1_la_LIBADD = $(top_builddir)/resrc/libflux-resrc.la \
    $(top_builddir)/src/common/libutil/libutil.la \
    $(FLUX_CORE_LIBS) $(JSON_LIBS) $(CZMQ_LIBS)
schedplugin1_la_LDFLAGS = $(fluxlib_ldflags) -avoid-version -module

backfillplugin1_la_SOURCES = backfillplugin1.c
backfillplugin1_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/resrc
backfillplugin1_la_LIBADD = $(top_builddir)/resrc/libflux-resrc.la \
    $(top_builddir)/src/common/libutil/libutil.la \
    $(FLUX_CORE_LIBS) $(JSON_LIBS) $(CZMQ_LIBS)
backfillplugin1_la_LDFLAGS = $(fluxlib_ldflags) -avoid-version -module

fluxcmd_PROGRAMS = flux-waitjob
flux_waitjob_SOURCES = flux-waitjob.c
flux_waitjob_CFLAGS = $(AM_CFLAGS)
flux_waitjob_LDADD = $(JSON_LIBS) $(FLUX_CORE_LIBS) \
    $(top_builddir)/src/common/libutil/libutil.la \
    $(CZMQ_LIBS)
