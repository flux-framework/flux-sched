include Makefile.inc

SUBDIRS = rdl resrc simulator sched t

.PHONY: all clean $(SUBDIRS)
all check clean: $(SUBDIRS)

all: TARGET="all"
clean: TARGET="clean"
check: all
	$(MAKE) TARGET="check"

$(SUBDIRS):
	@$(MAKE) -C $@ $(TARGET)

sched: rdl
simulator: rdl
t: rdl sched simulator

.PHONY: force
force: ;
