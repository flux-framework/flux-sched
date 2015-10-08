include ../Makefile.inc

CFLAGS = $(COMMON_CFLAGS) $(FLUX_CFLAGS) $(RESRC_CFLAGS)
LIBS = $(FLUX_LIBS) $(RESRC_LIBS)

CONF=../conf/hype.lua

BUILD = schedsrv.so schedplugin1.so flux-waitjob

all: $(BUILD)

schedsrv.so: schedsrv.o xzmalloc.o log.o jsonutil.o
	$(CC) -shared -o $@ $^ $(LIBS) $(SIM_LIBS)
schedplugin%.so: schedplugin%.o xzmalloc.o log.o jsonutil.o
	$(CC) -shared -o $@ $^ $(LIBS)
flux-waitjob: flux-waitjob.o log.o jsonutil.o
	$(CC) -o $@ $^ $(LIBS)

start:
	$(FLUX) start
load:
	$(FLUX) module load ./schedsrv.so rdl-conf=$(CONF)
unload:
	$(FLUX) module remove sched

check:
	echo "This is an empty dependency"

clean:
	rm -f *.o $(BUILD)
