include ../Makefile.inc

CFLAGS = $(COMMON_CFLAGS) $(FLUX_CFLAGS) $(RDL_CFLAGS)
LIBS = $(FLUX_LIBS) $(RDL_LIBS)

CONF=../conf/hype.lua

BUILD = schedsrv.so schedplugin1.so

all: $(BUILD)

schedsrv.so: schedsrv.o xzmalloc.o log.o jsonutil.o
	$(CC) -shared -o $@ $^ $(LIBS)
schedplugin1.so: schedplugin1.o xzmalloc.o log.o jsonutil.o
	$(CC) -shared -o $@ $^ $(LIBS)

start:
	$(FLUX) start -o,-Msched,-Osched:rdl-conf=$(CONF)
load:
	$(FLUX) module load sched rdl-conf=$(CONF)
unload:
	$(FLUX) module remove sched

clean:
	rm -f *.o $(BUILD)
