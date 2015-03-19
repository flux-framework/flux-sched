include ../Makefile.inc

SUBDIRS = test
CFLAGS = $(COMMON_CFLAGS) $(FLUX_CFLAGS) $(RDL_CFLAGS)
LIBS = $(FLUX_LIBS) $(RDL_LIBS)

X_OBJS_LIB = hostlist.o

BUILD = libresrc.so.0

all: $(BUILD)
	for subdir in $(SUBDIRS); do make -C $$subdir $@; done

libresrc.so.0: resrc.o resrc_tree.o $(X_OBJS_LIB)
	$(CC) -shared -Wl,--version-script=resrc_version.map -o $@ $^ $(LIBS)
	ln -sf $(shell basename $@) $(@:.0=)

check: libresrc.so.0
	for subdir in $(SUBDIRS); do make -C $$subdir $@; done

clean:
	rm -f *.so *.o $(BUILD)
	for subdir in $(SUBDIRS); do make -C $$subdir $@; done
