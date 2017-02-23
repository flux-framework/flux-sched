AM_CFLAGS = \
	@GCCWARN@ \
	$(CODE_COVERAGE_CFLAGS)

AM_CPPFLAGS = \
	$(CZMQ_CFLAGS)

AM_LDFLAGS = \
	$(CODE_COVERAGE_LDFLAGS)

noinst_LTLIBRARIES = libutil.la

libutil_la_SOURCES = \
	xzmalloc.c \
	xzmalloc.h \
	log.c \
	log.h \
	oom.h \
	shortjansson.h \
    compiler.h \
	interval_tree.c \
	interval_tree.h \
	interval_tree_generic.h \
	rbtree.c \
	rbtree.h \
	rbtree_augmented.h

