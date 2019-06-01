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

SUBDIRS = . test

noinst_PROGRAMS = grug2dot resource-query

grug2dot_SOURCES = \
    grug2dot.cpp
grug2dot_CXXFLAGS = \
    $(AM_CXXFLAGS)
grug2dot_LDADD = \
    ../libresource.la \
    $(BOOST_LDFLAGS) \
    $(BOOST_SYSTEM_LIB) \
    $(BOOST_FILESYSTEM_LIB) \
    $(BOOST_GRAPH_LIB) \
    $(BOOST_REGEX_LIB)

resource_query_SOURCES = \
    resource-query.cpp \
    command.cpp
resource_query_CXXFLAGS = \
    $(AM_CXXFLAGS)
resource_query_LDADD = \
    ../libresource.la \
    $(READLINE_LIBS) \
    $(BOOST_LDFLAGS) \
    $(BOOST_SYSTEM_LIB) \
    $(BOOST_FILESYSTEM_LIB) \
    $(BOOST_GRAPH_LIB) \
    $(BOOST_REGEX_LIB)
