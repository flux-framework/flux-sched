AM_CXXFLAGS = \
    $(WARNING_CXXFLAGS) \
    $(CODE_COVERAGE_CXXFLAGS)

AM_CFLAGS = \
	$(WARNING_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS)

AM_CPPFLAGS = \
	-I$(top_srcdir) $(CZMQ_CFLAGS) $(FLUX_CORE_CFLAGS) $(BOOST_CPPFLAGS)

AM_LDFLAGS = \
	$(CODE_COVERAGE_LIBS)

shellplugindir = \
	$(fluxlibdir)/shell/plugins

shellplugin_LTLIBRARIES = datastaging.la

datastaging_la_SOURCES = \
	data-staging.cpp

datastaging_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(fluxplugin_ldflags) \
	-module

datastaging_la_LIBADD= \
    $(BOOST_LDFLAGS) \
    $(BOOST_FILESYSTEM_LIB) \
    $(JANSSON_LIBS)
