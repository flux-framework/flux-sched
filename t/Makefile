include ../Makefile.inc

CFLAGS = $(COMMON_CFLAGS) $(FLUX_CFLAGS) $(RDL_CFLAGS)
LIBS = $(FLUX_LIBS) $(RDL_LIBS)

#
# The following variables are used by sharness.d/flux-sharness.sh
#
#
export FLUX_BUILD_DIR=$(FLUX_BUILDDIR)
export FLUX_SOURCE_DIR=$(FLUX_SRCDIR)

BUILD = 
TESTS = \
	t0000-sharness.t \
	t0001-basic.t \
	lua/t0001-rdl-basic.t \
	lua/t0002-multilevel.t \
	lua/t0003-default-tags.t \
	lua/t0004-derived-type.t \
	t0002-waitjob.t \
	t1000-jsc.t \
	t2000-fcfs.t \
	t2001-fcfs-aware.t \
	t2002-easy.t

all: $(BUILD)

check: $(TESTS)
	@export LUA_PATH="$$LUA_PATH;$(FLUX_BUILDDIR)/src/bindings/lua/?.lua;$(FLUX_BUILDDIR)/t/?.lua;$(RDL_LUA_PATH);;"; \
	export LUA_CPATH="$$LUA_CPATH;$(FLUX_BUILDDIR)/src/bindings/lua/?.so;$(RDL_LUA_CPATH);;"; \
        rc=0; \
	for t in $(TESTS); do \
		./$$t; \
		if test $$? -ne 0; then echo FAIL: $$t; rc=1; \
				   else echo PASS: $$t; fi; \
	done; \
	exit $$rc

clean:
	rm -rf *.o $(BUILD) test-results trash-directory*
