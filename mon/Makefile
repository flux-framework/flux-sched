include ../Makefile.inc

CFLAGS = $(COMMON_CFLAGS) $(FLUX_CFLAGS)
LIBS = $(FLUX_LIBS)

X_OBJS = log.o xzmalloc.o jsonutil.o

BUILD = flux-mon monsrv.so

all: $(BUILD)

flux-mon: flux-mon.o $(X_OBJS)
	$(CC) -o $@ $^ $(LIBS)

monsrv.so: monsrv.o $(X_OBJS)
	$(CC) -shared -o $@ $^ $(LIBS)

start:
	$(FLUX) start
load:
	$(FLUX) module load ./monsrv.so
unload:
	$(FLUX) module remove mon 

clean:
	rm -f *.o $(BUILD)
