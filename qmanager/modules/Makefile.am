AM_CXXFLAGS = \
    $(WARNING_CXXFLAGS) \
    $(CODE_COVERAGE_CXXFLAGS)

AM_LDFLAGS = \
    $(CODE_COVERAGE_LDFLAGS)

AM_CPPFLAGS = \
    -I$(top_srcdir) \
    $(FLUX_CORE_CFLAGS)

fluxmod_LTLIBRARIES = qmanager.la

#
# queue manager service module
#
qmanager_la_SOURCES = \
    qmanager.cpp \
    $(top_srcdir)/resource/hlapi/bindings/c++/reapi.hpp \
    $(top_srcdir)/resource/hlapi/bindings/c++/reapi_cli.hpp \
    $(top_srcdir)/resource/hlapi/bindings/c++/reapi_cli_impl.hpp \
    $(top_srcdir)/resource/hlapi/bindings/c++/reapi_module.hpp \
    $(top_srcdir)/resource/hlapi/bindings/c++/reapi_module_impl.hpp \
    $(top_srcdir)/qmanager/policies/base/queue_policy_base.hpp \
    $(top_srcdir)/qmanager/policies/base/queue_policy_base_impl.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_fcfs.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_fcfs_impl.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_bf_base.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_bf_base_impl.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_easy.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_easy_impl.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_factory.hpp \
    $(top_srcdir)/qmanager/policies/queue_policy_factory_impl.hpp

qmanager_la_CXXFLAGS = \
    $(AM_CXXFLAGS) \
    $(FLUX_CORE_CFLAGS)

qmanager_la_LIBADD = \
    $(top_builddir)/src/common/libschedutil/libschedutil.la \
    $(FLUX_CORE_LIBS) \
    $(CZMQ_LIBS) \
    $(JANSSON_LIBS)

qmanager_la_LDFLAGS = \
    $(AM_LDFLAGS) \
    $(fluxmod_ldflags) -module

