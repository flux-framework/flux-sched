AM_CFLAGS = @GCCWARN@ $(CODE_COVERAGE_CFLAGS)
AM_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

AM_CPPFLAGS = -I$(top_srcdir) \
    $(JSON_CFLAGS) $(CZMQ_CFLAGS) $(FLUX_CORE_CFLAGS)

schedplugin_ldflags = $(fluxlib_ldflags) -avoid-version -module \
    -Wl,--version-script=$(srcdir)/plugin_version.map

CONF = $(abs_top_srcdir)/conf/hype.lua
FLUX_MODULE_PATH = $(top_srcdir)/sched
LUA_PATH = $(abs_top_srcdir)/rdl/?.lua;$(FLUX_PREFIX)/share/lua/5.1/?.lua;$${LUA_PATH};;
LUA_CPATH = $(abs_top_builddir)/rdl/?.so;$${LUA_CPATH};;

fluxmod_LTLIBRARIES = sched.la

schedplugin_LTLIBRARIES = sched_fcfs.la \
                  sched_backfill.la

noinst_HEADERS = scheduler.h rs2rank.h rsreader.h plugin.h

sched_la_SOURCES = sched.c rs2rank.c rsreader.c plugin.c
sched_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/resrc
sched_la_LIBADD = $(top_builddir)/resrc/libflux-resrc.la \
    $(FLUX_CORE_LIBS) $(JSON_LIBS) $(CZMQ_LIBS) \
    $(top_builddir)/simulator/libflux-sim.la
sched_la_LDFLAGS = $(AM_LDFLAGS) $(fluxmod_ldflags) -module

sched_fcfs_la_SOURCES = sched_fcfs.c
sched_fcfs_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/resrc
sched_fcfs_la_LIBADD = $(top_builddir)/resrc/libflux-resrc.la \
    $(FLUX_CORE_LIBS) $(JSON_LIBS) $(CZMQ_LIBS)
sched_fcfs_la_LDFLAGS = $(AM_LDFLAGS) $(schedplugin_ldflags)

sched_backfill_la_SOURCES = sched_backfill.c
sched_backfill_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/resrc
sched_backfill_la_LIBADD = $(top_builddir)/resrc/libflux-resrc.la \
    $(FLUX_CORE_LIBS) $(JSON_LIBS) $(CZMQ_LIBS)
sched_backfill_la_LDFLAGS = $(AM_LDFLAGS) $(schedplugin_ldflags)

fluxcmd_PROGRAMS = flux-waitjob
flux_waitjob_SOURCES = flux-waitjob.c
flux_waitjob_CFLAGS = $(AM_CFLAGS)
flux_waitjob_LDADD = $(JSON_LIBS) $(FLUX_CORE_LIBS) \
    $(top_builddir)/src/common/libutil/libutil.la \
    $(CZMQ_LIBS)

EXTRA_DIST = plugin_version.map
