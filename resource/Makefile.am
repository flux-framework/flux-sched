AM_CXXFLAGS = \
    $(WARNING_CXXFLAGS) \
    -Wno-unused-local-typedefs \
    -Wno-deprecated-declarations \
    -Wno-unused-variable \
    -Wno-error \
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
    generators/spec.cpp \
    generators/spec.hpp
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
    policies/dfu_match_high_id_first.cpp \
    policies/dfu_match_low_id_first.cpp \
    policies/dfu_match_locality.cpp \
    jobinfo/jobinfo.cpp \
    schema/resource_data.cpp \
    schema/infra_data.cpp \
    schema/sched_data.cpp \
    schema/color.cpp \
    traversers/dfu.cpp \
    traversers/dfu_impl.cpp \
    policies/base/dfu_match_cb.cpp \
    policies/base/matcher.cpp \
    generators/gen.cpp \
    generators/spec.cpp \
    evaluators/scoring_api.cpp \
    evaluators/edge_eval_api.cpp \
    utilities/command.hpp \
    policies/dfu_match_high_id_first.hpp \
    policies/dfu_match_low_id_first.hpp \
    policies/dfu_match_locality.hpp \
    jobinfo/jobinfo.hpp \
    schema/resource_graph.hpp \
    schema/data_std.hpp \
    schema/infra_data.hpp \
    schema/sched_data.hpp \
    schema/resource_data.hpp \
    schema/color.hpp \
    traversers/dfu.hpp \
    traversers/dfu_impl.hpp \
    policies/base/dfu_match_cb.hpp \
    policies/base/matcher.hpp \
    generators/gen.hpp \
    generators/spec.hpp \
    evaluators/scoring_api.hpp \
    evaluators/edge_eval_api.hpp \
    evaluators/fold.hpp \
    config/system_defaults.hpp \
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

