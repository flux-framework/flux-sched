include ../Makefile.inc

SUBDIRS = test
CFLAGS = $(COMMON_CFLAGS) $(FLUX_CFLAGS) $(RDL_CFLAGS)
LIBS = $(FLUX_LIBS) $(RDL_LIBS) $(KVS_LIBS)
ALL_LIBS=$(LIBS) $(SIM_LIBS)

CONF=../conf/hype.lua
JOBDATA=job_data/hype-large.sorted.csv

COMMON_OBJS = xzmalloc.o log.o jsonutil.o

SCHEDULERS_C = $(wildcard sim_sched_*.c)
SCHEDULERS_SO = $(SCHEDULERS_C:sim_sched_%.c=sim_sched_%.so)
SCHEDULER_OBJS = scheduler.o

BUILD = simsrv.so submitsrv.so sim_execsrv.so $(SCHEDULERS_SO) libsim.so

all:  $(BUILD)
	for subdir in $(SUBDIRS); do make -C $$subdir $@; done

libsim.so: simulator.c
	$(CC) $(CFLAGS) -shared -o $@ $^ $(LIBS)

sim_sched_%.so: sim_sched_%.c $(COMMON_OBJS) $(SCHEDULER_OBJS) libsim.so
	$(CC) $(CFLAGS) -shared -o $@ $^ $(ALL_LIBS)

%srv.so : %srv.o $(COMMON_OBJS) libsim.so
	$(CC) $(CFLAGS) -shared -o $@ $^ $(ALL_LIBS)

start:
	$(FLUX) -B ../.build/src/broker/flux-broker start
load:
	$(FLUX) module load ./simsrv.so exit-on-complete=true
	$(FLUX) module load ./submitsrv.so job-csv=$(JOBDATA)
	$(FLUX) module load ./sim_execsrv.so
unload:
	$(FLUX) module remove sim_exec
	$(FLUX) module remove submit
	$(FLUX) module remove sim
	$(FLUX) module remove sim_sched
sim_sched_%: sim_sched_%.so load
	$(FLUX) module load ./$@.so rdl-conf=$(CONF)
sched: load
	$(FLUX) module load ../sched/schedsrv.so rdl-conf=$(CONF) in-sim=true

check: all
	for subdir in $(SUBDIRS); do make -C $$subdir $@; done

clean:
	rm -f *.a *.o *.so
	for subdir in $(SUBDIRS); do make -C $$subdir $@; done

## Debugging convienences
SCHED=../sched/schedsrv.so
EXEC_MOD=--module ./sim_execsrv.so
SIM_MOD=--module ./simsrv.so --modopt sim:save-path=${SAVEPATH}
SUBMIT_MOD=--module ./submitsrv.so --modopt submit:job-csv=${JOBDATA}
#SCHED_MOD=--module ./${SCHED} --modopt sim_sched:rdl-conf=${CONF}
SCHED_MOD=--module ./${SCHED} --modopt sched:rdl-conf=${CONF} --modopt sched:in-sim=true
BROKER_OPTS=-Lstderr ${EXEC_MOD} ${SIM_MOD} ${SUBMIT_MOD} ${SCHED_MOD} --boot-method=SINGLE
BROKER=$(FLUX_BUILDDIR)/src/broker/.libs/flux-broker

VALGRIND=$(shell which valgrind)
TOTALVIEW=$(shell which totalview)
GDB=$(shell which gdb)
LLDB=$(shell which lldb)

callgrind:
	$(FLUX) $(VALGRIND) --tool=callgrind --trace-children=yes $(BROKER) $(BROKER_OPTS)
valgrind:
	$(FLUX)	$(VALGRIND) --log-file="%p.valgrind.out" --tool=memcheck --leak-check=full --show-leak-kinds=all --trace-children=yes $(BROKER) $(BROKER_OPTS)
gdb:
	$(FLUX) $(GDB) --args $(BROKER) $(BROKER_OPTS)
lldb:
	$(FLUX) $(LLDB) -- $(BROKER) $(BROKER_OPTS)
totalview:
	$(FLUX) $(TOTALVIEW) --args $(BROKER) $(BROKER_OPTS)


.PHONY: all clean load unload start
