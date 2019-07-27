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

fluxmod_LTLIBRARIES = fluxion-resource.la

#
# resource service module
#
fluxion_resource_la_SOURCES = \
    resource_match.cpp
fluxion_resource_la_CXXFLAGS = \
    $(AM_CXXFLAGS) \
    $(JOBSPEC_CFLAGS) \
    $(FLUX_CORE_CFLAGS)
fluxion_resource_la_LIBADD = \
    ../libresource.la \
    $(FLUX_CORE_LIBS) \
    $(DL_LIBS) \
    $(HWLOC_LIBS) \
    $(UUID_LIBS) \
    $(JANSSON_LIBS) \
    $(CZMQ_LIBS) \
    $(BOOST_LDFLAGS)
fluxion_resource_la_LDFLAGS = \
    $(AM_LDFLAGS) \
    $(fluxmod_ldflags) -module

