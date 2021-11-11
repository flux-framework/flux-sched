AM_CXXFLAGS = \
    $(WARNING_CXXFLAGS) \
    $(CODE_COVERAGE_CXXFLAGS)

AM_LDFLAGS = \
    $(CODE_COVERAGE_LDFLAGS)

AM_CPPFLAGS = \
    -I$(top_srcdir) \
    $(FLUX_CORE_CFLAGS) \
    $(FLUX_SCHEDUTIL_CFLAGS)

fluxmod_LTLIBRARIES = sched-fluxion-qmanager.la

#
# queue manager service module
#
sched_fluxion_qmanager_la_SOURCES = \
    qmanager.cpp \
    qmanager_opts.cpp \
    qmanager_opts.hpp \
    qmanager_callbacks.cpp \
    qmanager_callbacks.hpp \
    $(top_srcdir)/src/common/liboptmgr/optmgr.hpp \
    $(top_srcdir)/src/common/liboptmgr/optmgr_impl.hpp \
    $(top_srcdir)/resource/libjobspec/jobspec.hpp \
    $(top_srcdir)/resource/hlapi/bindings/c++/reapi.hpp \
    $(top_srcdir)/resource/hlapi/bindings/c++/reapi_module.hpp \
    $(top_srcdir)/resource/hlapi/bindings/c++/reapi_module_impl.hpp \
    $(top_srcdir)/qmanager/config/queue_system_defaults.hpp \
    $(top_srcdir)/qmanager/policies/base/queue_policy_base.hpp \
    $(top_srcdir)/qmanager/policies/base/queue_policy_base_impl.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_fcfs.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_fcfs_impl.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_bf_base.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_bf_base_impl.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_easy.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_easy_impl.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_hybrid.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_hybrid_impl.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_conservative.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_conservative_impl.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_factory.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_factory_impl.hpp \
    $(top_srcdir)/src/common/c++wrappers/eh_wrapper.hpp

sched_fluxion_qmanager_la_CXXFLAGS = \
    $(AM_CXXFLAGS) \
    $(FLUX_CORE_CFLAGS) \
    $(FLUX_SCHEDUTIL_CFLAGS)

sched_fluxion_qmanager_la_LIBADD = \
    $(top_builddir)/resource/libjobspec/libjobspec_conv.la \
    $(FLUX_CORE_LIBS) \
    $(FLUX_SCHEDUTIL_LIBS) \
    $(CZMQ_LIBS) \
    $(JANSSON_LIBS)

sched_fluxion_qmanager_la_LDFLAGS = \
    $(AM_LDFLAGS) \
    $(fluxmod_ldflags) -module

