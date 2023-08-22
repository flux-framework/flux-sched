SUBDIRS = .

AM_CFLAGS = \
	$(WARNING_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS)

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	$(CZMQ_CFLAGS)

AM_LDFLAGS = \
	$(CODE_COVERAGE_LDFLAGS)

noinst_LTLIBRARIES = libplanner.la
noinst_HEADERS = \
	planner.h \
	planner_multi.h \
	$(top_srcdir)/resource/planner/c++/planner.hpp \
	$(top_srcdir)/resource/planner/c++/planner_multi.hpp \
	$(top_srcdir)/resource/planner/c++/planner_internal_tree.hpp \
	$(top_srcdir)/resource/planner/c++/scheduled_point_tree.hpp \
	$(top_srcdir)/resource/planner/c++/mintime_resource_tree.hpp \
	$(top_srcdir)/src/common/yggdrasil/bst.hpp \
	$(top_srcdir)/src/common/yggdrasil/bst.cpp \
	$(top_srcdir)/src/common/yggdrasil/debug.hpp \
	$(top_srcdir)/src/common/yggdrasil/options.hpp \
	$(top_srcdir)/src/common/yggdrasil/rbtree.hpp \
	$(top_srcdir)/src/common/yggdrasil/rbtree.cpp \
	$(top_srcdir)/src/common/yggdrasil/size_holder.hpp \
	$(top_srcdir)/src/common/yggdrasil/tree_iterator.hpp \
	$(top_srcdir)/src/common/yggdrasil/tree_iterator.cpp \
	$(top_srcdir)/src/common/yggdrasil/util.hpp

libplanner_la_SOURCES = \
	planner_c_interface.cpp \
	planner_multi_c_interface.cpp

libplanner_la_LIBADD = \
	$(top_builddir)/resource/planner/c++/libplanner_cxx.la

libplanner_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/resource/planner/c -I$(top_srcdir)/resource/planner/c++

libplanner_la_CXXFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/resource/planner/c -I$(top_srcdir)/resource/planner/c++

