include Makefile.inc

SUBDIRS = rdl sched simulator t

all:

check: all

all clean install check:
	for subdir in $(SUBDIRS); do make -C $$subdir $@; done

