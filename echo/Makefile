include ../Makefile.inc

CFLAGS = $(COMMON_CFLAGS) $(FLUX_CFLAGS)
LIBS = $(FLUX_LIBS)

X_OBJS =jsonutil.o log.o xzmalloc.o monotime.o

BUILD = echo echosrv.so

all: $(BUILD)

echo: echo.o $(X_OBJS)
	$(CC) -o $@ $^ $(LIBS)

echosrv.so: echosrv.o $(X_OBJS)
	$(CC) -shared -o $@ $^ $(LIBS)

start:
	$(FLUX) start -o,-Mecho
load:
	$(FLUX) module load echo
unload:
	$(FLUX) module remove echo

clean:
	rm -f *.o $(BUILD)
