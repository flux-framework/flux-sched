AM_CFLAGS = @GCCWARN@

AM_CPPFLAGS = $(JSON_CFLAGS) $(CZMQ_CFLAGS) $(FLUX_CORE_CFLAGS) \
    -I $(FLUX_BUILDDIR)

SUBDIRS = . test

CONF = $(top_srcdir)/conf/hype.lua
JOBDATA=job_data/hype.csv

lib_LTLIBRARIES = libflux-sim.la
fluxmod_LTLIBRARIES = simsrv.la sim_execsrv.la submitsrv.la sim_sched_fcfs.la \
    sim_sched_fcfs_aware.la sim_sched_easy.la

fluxschedinclude_HEADERS = simulator.h scheduler.h

libflux_sim_la_SOURCES = simulator.c
libflux_sim_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/rdl
libflux_sim_la_LIBADD = $(FLUX_CORE_LIBS) \
    $(top_builddir)/foreign/libforeign.la \
    $(top_builddir)/src/common/libutil/libutil.la \
    $(LUA_LIB) $(JSON_LIBS) $(CZMQ_LIBS)
libflux_sim_la_LDFLAGS = $(fluxlib_ldflags)

simsrv_la_SOURCES = simsrv.c
simsrv_la_CFLAGS = $(AM_CFLAGS) $(AM_CPPFLAGS)
simsrv_la_LIBADD = $(top_builddir)/resrc/libflux-resrc.la libflux-sim.la \
    $(FLUX_CORE_LIBS) $(JSON_LIBS) $(CZMQ_LIBS)
simsrv_la_LDFLAGS = $(fluxmod_ldflags) -module

sim_execsrv_la_SOURCES = sim_execsrv.c
sim_execsrv_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/rdl
sim_execsrv_la_LIBADD = $(top_builddir)/rdl/libflux-rdl.la \
    $(top_builddir)/resrc/libflux-resrc.la libflux-sim.la \
    $(FLUX_CORE_LIBS) $(JSON_LIBS) $(CZMQ_LIBS)
sim_execsrv_la_LDFLAGS = $(fluxmod_ldflags) -module

submitsrv_la_SOURCES = submitsrv.c
submitsrv_la_CFLAGS = $(AM_CFLAGS)
submitsrv_la_LIBADD = $(top_builddir)/resrc/libflux-resrc.la libflux-sim.la \
    $(FLUX_CORE_LIBS) $(JSON_LIBS) $(CZMQ_LIBS)
submitsrv_la_LDFLAGS = $(fluxmod_ldflags) -module

sim_sched_fcfs_la_SOURCES = sim_sched_fcfs.c scheduler.c
sim_sched_fcfs_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/rdl
sim_sched_fcfs_la_LIBADD = $(top_builddir)/rdl/libflux-rdl.la \
    $(top_builddir)/resrc/libflux-resrc.la libflux-sim.la \
    $(FLUX_CORE_LIBS) $(JSON_LIBS) $(CZMQ_LIBS)
sim_sched_fcfs_la_LDFLAGS = $(fluxmod_ldflags) -module

sim_sched_fcfs_aware_la_SOURCES = sim_sched_fcfs_aware.c scheduler.c
sim_sched_fcfs_aware_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/rdl
sim_sched_fcfs_aware_la_LIBADD = $(top_builddir)/rdl/libflux-rdl.la \
    $(top_builddir)/resrc/libflux-resrc.la libflux-sim.la \
    $(FLUX_CORE_LIBS) $(JSON_LIBS) $(CZMQ_LIBS)
sim_sched_fcfs_aware_la_LDFLAGS = $(fluxmod_ldflags) -module

sim_sched_easy_la_SOURCES = sim_sched_easy.c scheduler.c
sim_sched_easy_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/rdl
sim_sched_easy_la_LIBADD = $(top_builddir)/rdl/libflux-rdl.la \
    $(top_builddir)/resrc/libflux-resrc.la libflux-sim.la \
    $(FLUX_CORE_LIBS) $(JSON_LIBS) $(CZMQ_LIBS)
sim_sched_easy_la_LDFLAGS = $(fluxmod_ldflags) -module
