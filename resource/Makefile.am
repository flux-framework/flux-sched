AM_CXXFLAGS = \
    $(WARNING_CXXFLAGS) \
    -Wno-unused-local-typedefs \
    -Wno-deprecated-declarations \
    -Wno-unused-variable \
    -Wno-maybe-uninitialized \
    $(CODE_COVERAGE_CXXFLAGS)

AM_LDFLAGS = $(CODE_COVERAGE_LDFLAGS) \
             $(BOOST_LDFLAGS)

AM_CPPFLAGS = -I$(top_srcdir) $(CZMQ_CFLAGS) $(FLUX_CORE_CFLAGS) \
	      $(BOOST_CPPFLAGS)

SUBDIRS = planner .

noinst_PROGRAMS = utilities/grug2dot utilities/resource-query

EXTRA_DIST= \
    utilities/conf

#
# grug2dot
#
utilities_grug2dot_SOURCES = \
    utilities/grug2dot.cpp \
    resource_gen_spec.cpp \
    resource_gen_spec.hpp
utilities_grug2dot_CXXFLAGS = \
    $(AM_CXXFLAGS)
utilities_grug2dot_LDADD = \
    $(BOOST_SYSTEM_LIB) \
    $(BOOST_FILESYSTEM_LIB) \
    $(BOOST_GRAPH_LIB) \
    $(BOOST_REGEX_LIB)

#.
# resource-query
#
utilities_resource_query_SOURCES = \
    utilities/resource-query.cpp \
    utilities/command.cpp \
    dfu_traverse.cpp \
    dfu_traverse_impl.cpp \
    resource_gen.cpp \
    resource_gen_spec.cpp \
    utilities/command.hpp \
    dfu_match_id_based.hpp \
    resource_graph.hpp \
    resource_data.hpp \
    resource_gen.hpp \
    resource_gen_spec.hpp \
    dfu_traverse.hpp \
    dfu_traverse_impl.hpp \
    dfu_match_cb.hpp \
    matcher_data.hpp \
    system_defaults.hpp \
    scoring_api.hpp \
    planner/planner.h
utilities_resource_query_CXXFLAGS = \
    $(AM_CXXFLAGS) \
    $(JOBSPEC_CFLAGS)
utilities_resource_query_LDADD = \
    $(top_builddir)/resource/planner/libplanner.la \
    $(JOBSPEC_LIBS) \
    $(READLINE_LIBS) \
    $(BOOST_SYSTEM_LIB) \
    $(BOOST_FILESYSTEM_LIB) \
    $(BOOST_GRAPH_LIB) \
    $(BOOST_REGEX_LIB)

