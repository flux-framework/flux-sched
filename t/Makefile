include ../Makefile.inc

CFLAGS = $(COMMON_CFLAGS) $(FLUX_CFLAGS) $(RDL_CFLAGS)
LIBS = $(FLUX_LIBS) $(RDL_LIBS)

#
# The following variables are used by shareness.d/flux-shareness.sh
#
#
export FLUX_BUILD_DIR=$(FLUX_BUILDDIR)
export FLUX_SOURCE_DIR=$(FLUX_SRCDIR)

BUILD = 
TESTS = \
	t0000-sharness.t \
	t0001-basic.t

all: $(BUILD)

check: $(TESTS)
	for t in $(TESTS); do \
	$$t --debug; done 

clean:
	rm -rf *.o $(BUILD) test-results trash-directory*
