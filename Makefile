include Makefile.inc

SUBDIRS = rdl sched simulator

all clean install:
	for subdir in $(SUBDIRS); do make -C $$subdir $@; done
