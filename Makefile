include Makefile.inc

SUBDIRS = rdl sched simulator t

all clean install:
	for subdir in $(SUBDIRS); do make -C $$subdir $@; done

