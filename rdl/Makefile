include ../Makefile.inc

CFLAGS = $(COMMON_CFLAGS) $(FLUX_CFLAGS)
LIBS = $(FLUX_LIBS)

X_OBJS_CMD = optparse.o list.o
X_OBJS_LIB = json-lua.o list.o

BUILD = flux-rdltool librdl.so.0 cpuset.so

all: $(BUILD)

flux-rdltool: flux-rdltool.o $(X_OBJS_CMD) librdl.so.0
	$(CC) -o $@ $^ $(LIBS) $(RDL_LIBS)

librdl.so.0: rdl.o $(X_OBJS_LIB)
	$(CC) -shared -Wl,--version-script=rdl_version.map -o $@ $^ $(LIBS)
	ln -sf $(shell basename $@) $(@:.0=)

cpuset.so: lua-cpuset.o cpuset-str.o
	$(CC) -shared -o $@ $^

check: 
	echo "This is an empty dependency"

clean:
	rm -f *.so *.o $(BUILD)

dump:
	$(FLUX) ./flux-rdltool -f ../conf/hype.lua  tree
