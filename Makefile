include Makefile.inc

SUBDIRS = echo rdl sched

all clean install:
	for subdir in $(SUBDIRS); do make -C $$subdir $@; done
