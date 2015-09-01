include ../Makefile.inc

CFLAGS = $(COMMON_CFLAGS) $(FLUX_CFLAGS)
LIBS = $(FLUX_LIBS) -luuid

BUILD = libschedcomm.so.0

all: $(BUILD)

libschedcomm.so.0: base64.o hostlist.o list.o json-lua.o jsonutil.o log.o \
	optparse.o tap.o xzmalloc.o
	$(CC) -shared -Wl,--version-script=common_version.map -o $@ $^ $(LIBS)
	ln -sf $(shell basename $@) $(@:.0=)

check:
	echo "This is an empty dependency"

clean:
	rm -f *.so *.o $(BUILD)

