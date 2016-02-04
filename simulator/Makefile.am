AM_CFLAGS = @GCCWARN@

AM_CPPFLAGS = $(JSON_CFLAGS) $(CZMQ_CFLAGS) $(FLUX_CORE_CFLAGS) \
    -I $(FLUX_BUILDDIR)

SUBDIRS = . test

CONF = ../conf/hype.lua
JOBDATA=job_data/hype.csv

lib_LTLIBRARIES = libsim.la simsrv.la sim_execsrv.la submitsrv.la \
                  sim_sched_fcfs.la sim_sched_fcfs_aware.la sim_sched_easy.la
noinst_LTLIBRARIES = libforeign.la

include_HEADERS = simulator.h

libforeign_la_SOURCES = \
    $(FLUX_BUILDDIR)/src/common/libutil/base64.c \
    $(FLUX_BUILDDIR)/src/common/libutil/jsonutil.c \
    $(FLUX_BUILDDIR)/src/common/libutil/log.c \
    $(FLUX_BUILDDIR)/src/common/libutil/xzmalloc.c

libsim_la_SOURCES = simulator.c
libsim_la_CFLAGS = $(AM_CFLAGS) -I../rdl
libsim_la_LIBADD = $(FLUX_CORE_LIBS) libforeign.la
libsim_la_LDFLAGS = -version-info 0:0:0

simsrv_la_SOURCES = simsrv.c
simsrv_la_CFLAGS = $(AM_CFLAGS)
simsrv_la_LIBADD = ../resrc/libresrc.la libsim.la
simsrv_la_LDFLAGS = -module -version-info 0:0:0

sim_execsrv_la_SOURCES = sim_execsrv.c
sim_execsrv_la_CFLAGS = $(AM_CFLAGS) -I../rdl
sim_execsrv_la_LIBADD = ../resrc/libresrc.la  $(FLUX_CORE_LIBS) libsim.la
sim_execsrv_la_LDFLAGS = -module -version-info 0:0:0

submitsrv_la_SOURCES = submitsrv.c
submitsrv_la_CFLAGS = $(AM_CFLAGS)
submitsrv_la_LIBADD = ../resrc/libresrc.la libsim.la
submitsrv_la_LDFLAGS = -module -version-info 0:0:0

sim_sched_fcfs_la_SOURCES = sim_sched_fcfs.c
sim_sched_fcfs_la_CFLAGS = $(AM_CFLAGS) -I../rdl
sim_sched_fcfs_la_LIBADD = ../resrc/libresrc.la libsim.la
sim_sched_fcfs_la_LDFLAGS = -module -version-info 0:0:0

sim_sched_fcfs_aware_la_SOURCES = sim_sched_fcfs_aware.c
sim_sched_fcfs_aware_la_CFLAGS = $(AM_CFLAGS) -I../rdl
sim_sched_fcfs_aware_la_LIBADD = ../resrc/libresrc.la libsim.la
sim_sched_fcfs_aware_la_LDFLAGS = -module -version-info 0:0:0

sim_sched_easy_la_SOURCES = sim_sched_easy.c
sim_sched_easy_la_CFLAGS = $(AM_CFLAGS) -I../rdl
sim_sched_easy_la_LIBADD = ../resrc/libresrc.la libsim.la
sim_sched_easy_la_LDFLAGS = -module -version-info 0:0:0
