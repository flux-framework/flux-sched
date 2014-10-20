include ../Makefile.inc

CFLAGS = $(COMMON_CFLAGS) $(FLUX_CFLAGS) $(RDL_CFLAGS)
LIBS = $(FLUX_LIBS) $(RDL_LIBS)

CONF=../conf/hype.lua
JOBDATA=job_data/2014-06-01.csv

COMMON_OBJS = simulator.o xzmalloc.o log.o jsonutil.o

BUILD = sim.so submit.so sim_sched.so sim_exec.so

all:  $(BUILD)

sim.so: simsrv.o $(COMMON_OBJS)
	$(CC) -shared -o $@ $^ $(LIBS)
submit.so: submitsrv.o $(COMMON_OBJS)
	$(CC) -shared -o $@ $^ $(LIBS)
sim_sched.so: sim_schedsrv.o $(COMMON_OBJS)
	$(CC) -shared -o $@ $^ $(LIBS)
sim_exec.so: sim_execsrv.o $(COMMON_OBJS)
	$(CC) -shared -o $@ $^ $(LIBS)

# N.B. "race fix" in job module is conditionally disabled
# turn it on there if needed until there is a proper fix for the race

start:
	$(FLUX) start
load:
	$(FLUX) module load ./sim.so
	$(FLUX) module load ./submit.so job-csv=$(JOBDATA)
	$(FLUX) module load ./sim_sched.so rdl-conf=$(CONF)
	$(FLUX) module load ./sim_exec.so
unload:
	$(FLUX) module load sim_exec
	$(FLUX) module load sim_sched
	$(FLUX) module load submit
	$(FLUX) module load sim
	

clean:
	rm -f *.a *.o

.PHONY: all clean load unload start
