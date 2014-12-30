include Makefile.inc

SUBDIRS = rdl resrc sched simulator t

all clean install check:
	for subdir in $(SUBDIRS); do make -C $$subdir $@; done

