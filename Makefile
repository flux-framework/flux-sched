include Makefile.inc

SUBDIRS = echo rdl sched simulator

all clean install:
	for subdir in $(SUBDIRS); do make -C $$subdir $@; done
