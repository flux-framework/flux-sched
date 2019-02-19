AM_CXXFLAGS = \
    $(WARNING_CXXFLAGS) \
    -Wno-unused-local-typedefs \
    -Wno-deprecated-declarations \
    -Wno-unused-variable \
    -Wno-error \
    $(CODE_COVERAGE_CXXFLAGS)

AM_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

AM_CPPFLAGS = -I$(top_srcdir) $(CZMQ_CFLAGS) $(FLUX_CORE_CFLAGS) \
	      $(BOOST_CPPFLAGS)

SUBDIRS = libjobspec planner . utilities modules

noinst_LTLIBRARIES = libresource.la

EXTRA_DIST= \
    utilities/conf

libresource_la_SOURCES = \
    policies/dfu_match_high_id_first.cpp \
    policies/dfu_match_low_id_first.cpp \
    policies/dfu_match_locality.cpp \
    policies/dfu_match_policy_factory.cpp \
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
    emitters/match_emitters.cpp \
    utilities/command.hpp \
    policies/dfu_match_high_id_first.hpp \
    policies/dfu_match_low_id_first.hpp \
    policies/dfu_match_locality.hpp \
    policies/dfu_match_policy_factory.hpp \
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
    emitters/match_emitters.hpp \
    planner/planner.h

libresource_la_CXXFLAGS = \
    $(WARNING_CXXFLAGS) \
    $(CODE_COVERAGE_CFLAGS) \
    $(AM_CXXFLAGS)

libresource_la_LIBADD = \
    $(top_builddir)/resource/planner/libplanner.la \
    $(top_builddir)/resource/libjobspec/libjobspec_conv.la \
    $(BOOST_LDFLAGS) \
    $(BOOST_SYSTEM_LIB) \
    $(BOOST_FILESYSTEM_LIB) \
    $(BOOST_GRAPH_LIB) \
    $(BOOST_REGEX_LIB) \
    $(HWLOC_LIBS)
