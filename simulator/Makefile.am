AM_CFLAGS = $(WARNING_CFLAGS) $(CODE_COVERAGE_CFLAGS)
AM_LDFLAGS = $(CODE_COVERAGE_LIBS)

AM_CPPFLAGS = -I$(top_srcdir) \
    $(JANSSON_CFLAGS) $(CZMQ_CFLAGS) $(FLUX_CORE_CFLAGS)

CONF = $(top_srcdir)/conf/hype.lua
JOBDATA=job_data/hype.csv

noinst_LTLIBRARIES = libflux-sim.la \
    simsrv.la sim_execsrv.la submitsrv.la

noinst_HEADERS = simulator.h

libflux_sim_la_SOURCES = simulator.c
libflux_sim_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/rdl
libflux_sim_la_LIBADD = $(FLUX_CORE_LIBS) \
    $(DL_LIBS) $(HWLOC_LIBS) $(UUID_LIBS) \
    $(LUA_LIB) $(JANSSON_LIBS) $(CZMQ_LIBS)
libflux_sim_la_LDFLAGS = $(AM_LDFLAGS) $(fluxlib_ldflags) \
    -Wl,--version-script=$(srcdir)/sim_version.map

simsrv_la_SOURCES = simsrv.c
simsrv_la_CFLAGS = $(AM_CFLAGS) $(AM_CPPFLAGS)
simsrv_la_LIBADD = $(top_builddir)/resrc/libflux-resrc.la libflux-sim.la \
    $(FLUX_CORE_LIBS) $(DL_LIBS) $(HWLOC_LIBS) $(UUID_LIBS) \
    $(JANSSON_LIBS) $(CZMQ_LIBS)
simsrv_la_LDFLAGS = $(AM_LDFLAGS) $(fluxmod_ldflags) -module -rpath /nowhere

sim_execsrv_la_SOURCES = sim_execsrv.c
sim_execsrv_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/rdl
sim_execsrv_la_LIBADD =  \
    $(top_builddir)/resrc/libflux-resrc.la libflux-sim.la \
    $(FLUX_CORE_LIBS) $(DL_LIBS) $(HWLOC_LIBS) $(UUID_LIBS) \
    $(JANSSON_LIBS) $(CZMQ_LIBS)
sim_execsrv_la_LDFLAGS = $(AM_LDFLAGS) $(fluxmod_ldflags) -module -rpath /nowhere

submitsrv_la_SOURCES = submitsrv.c
submitsrv_la_CFLAGS = $(AM_CFLAGS)
submitsrv_la_LIBADD = $(top_builddir)/resrc/libflux-resrc.la libflux-sim.la \
    $(FLUX_CORE_LIBS) $(DL_LIBS) $(HWLOC_LIBS) $(UUID_LIBS) \
    $(JANSSON_LIBS) $(CZMQ_LIBS)
submitsrv_la_LDFLAGS = $(AM_LDFLAGS) $(fluxmod_ldflags) -module -rpath /nowhere

EXTRA_DIST = sim_version.map
