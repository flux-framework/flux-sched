SUBDIRS = .

AM_CFLAGS = \
        $(WARNING_CFLAGS) \
        $(CODE_COVERAGE_CFLAGS)

AM_CPPFLAGS = \
        -I$(top_srcdir) \
        $(CZMQ_CFLAGS)

AM_LDFLAGS = \
        $(CODE_COVERAGE_LDFLAGS)

noinst_LTLIBRARIES = libplanner_cxx.la
noinst_HEADERS = \
        planner.hpp \
        planner_multi.hpp \
        planner_internal_tree.hpp \
        scheduled_point_tree.hpp \
        mintime_resource_tree.hpp \
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

libplanner_cxx_la_SOURCES = \
        planner.cpp \
        planner_multi.cpp \
        scheduled_point_tree.cpp \
        mintime_resource_tree.cpp \
        planner_internal_tree.cpp

libplanner_cxx_la_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/resource/planner/c++

libplanner_cxx_la_CXXFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/resource/planner/c++

