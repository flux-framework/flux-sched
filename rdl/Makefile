include ../Makefile.inc

SUBDIRS = test
CFLAGS = $(COMMON_CFLAGS) $(SCHEDCOMM_CFLAGS) $(FLUX_CFLAGS)
LIBS = $(SCHEDCOMM_LIBS) $(FLUX_LIBS)

BUILD = flux-rdltool librdl.so.0 cpuset.so

all: $(BUILD)
	for subdir in $(SUBDIRS); do make -C $$subdir $@; done

flux-rdltool: flux-rdltool.o librdl.so.0
	$(CC) -o $@ $^ $(LIBS) $(RDL_LIBS)

librdl.so.0: rdl.o
	$(CC) -shared -Wl,--version-script=rdl_version.map -o $@ $^ $(LIBS)
	ln -sf $(shell basename $@) $(@:.0=)

cpuset.so: lua-cpuset.o cpuset-str.o
	$(CC) -shared -o $@ $^

check: 
	echo "This is an empty dependency"

clean:
	rm -f *.so *.o $(BUILD)
	for subdir in $(SUBDIRS); do make -C $$subdir $@; done

dump:
	$(FLUX) ./flux-rdltool -f ../conf/hype.lua  tree
