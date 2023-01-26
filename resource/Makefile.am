AM_CXXFLAGS = \
    $(WARNING_CXXFLAGS) \
    $(CODE_COVERAGE_CXXFLAGS)

AM_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

AM_CPPFLAGS = -I$(top_srcdir) $(CZMQ_CFLAGS) $(FLUX_CORE_CFLAGS) \
	      $(BOOST_CPPFLAGS)

SUBDIRS = libjobspec planner . utilities modules reapi evaluators policies

noinst_LTLIBRARIES = libresource.la

EXTRA_DIST= \
    utilities/conf

libresource_la_SOURCES = \
    policies/dfu_match_locality.cpp \
    policies/dfu_match_var_aware.cpp \
    policies/dfu_match_policy_factory.cpp \
    jobinfo/jobinfo.cpp \
    schema/resource_data.cpp \
    schema/infra_data.cpp \
    schema/sched_data.cpp \
    schema/color.cpp \
    schema/ephemeral.cpp \
    traversers/dfu.cpp \
    traversers/dfu_impl.cpp \
    traversers/dfu_impl_update.cpp \
    policies/base/dfu_match_cb.cpp \
    policies/base/matcher.cpp \
    readers/resource_namespace_remapper.cpp \
    readers/resource_reader_base.cpp \
    readers/resource_spec_grug.cpp \
    readers/resource_reader_grug.cpp \
    readers/resource_reader_hwloc.cpp \
    readers/resource_reader_jgf.cpp \
    readers/resource_reader_rv1exec.cpp \
    readers/resource_reader_factory.cpp \
    evaluators/scoring_api.cpp \
    evaluators/edge_eval_api.cpp \
    evaluators/expr_eval_api.cpp \
    evaluators/expr_eval_vtx_target.cpp \
    writers/match_writers.cpp \
    store/resource_graph_store.cpp \
    utilities/command.hpp \
    policies/dfu_match_high_id_first.hpp \
    policies/dfu_match_low_id_first.hpp \
    policies/dfu_match_multilevel_id.hpp \
    policies/dfu_match_multilevel_id_impl.hpp \
    policies/dfu_match_locality.hpp \
    policies/dfu_match_var_aware.hpp \
    policies/dfu_match_policy_factory.hpp \
    jobinfo/jobinfo.hpp \
    schema/resource_graph.hpp \
    schema/data_std.hpp \
    schema/infra_data.hpp \
    schema/sched_data.hpp \
    schema/resource_base.hpp \
    schema/resource_data.hpp \
    schema/color.hpp \
    schema/ephemeral.hpp \
    traversers/dfu.hpp \
    traversers/dfu_impl.hpp \
    policies/base/dfu_match_cb.hpp \
    policies/base/matcher.hpp \
    readers/resource_namespace_remapper.hpp \
    readers/resource_reader_base.hpp \
    readers/resource_spec_grug.hpp \
    readers/resource_reader_grug.hpp \
    readers/resource_reader_hwloc.hpp \
    readers/resource_reader_jgf.hpp \
    readers/resource_reader_rv1exec.hpp \
    readers/resource_reader_factory.hpp \
    evaluators/scoring_api.hpp \
    evaluators/edge_eval_api.hpp \
    evaluators/fold.hpp \
    evaluators/expr_eval_api.hpp \
    evaluators/expr_eval_target.hpp \
    evaluators/expr_eval_vtx_target.hpp \
    config/system_defaults.hpp \
    writers/match_writers.hpp \
    store/resource_graph_store.hpp \
    planner/planner.h

libresource_la_CXXFLAGS = \
    $(WARNING_CXXFLAGS) \
    $(CODE_COVERAGE_CFLAGS) \
    $(AM_CXXFLAGS) \
    $(FLUX_HOSTLIST_CFLAGS) \
    $(FLUX_IDSET_CFLAGS)

libresource_la_LIBADD = \
    $(top_builddir)/resource/planner/libplanner.la \
    $(top_builddir)/resource/libjobspec/libjobspec_conv.la \
    $(FLUX_HOSTLIST_LIBS) \
    $(FLUX_IDSET_LIBS) \
    $(BOOST_LDFLAGS) \
    $(BOOST_SYSTEM_LIB) \
    $(BOOST_FILESYSTEM_LIB) \
    $(BOOST_GRAPH_LIB) \
    $(BOOST_REGEX_LIB) \
    $(HWLOC_LIBS) \
    $(JANSSON_LIBS)
