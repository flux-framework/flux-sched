AM_CXXFLAGS = \
	$(WARNING_CXXFLAGS) \
	$(CODE_COVERAGE_CXXFLAGS)

AM_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

AM_CPPFLAGS = -I$(top_srcdir)

TESTS = matcher_util_api_test01

check_PROGRAMS = $(TESTS)
matcher_util_api_test01_SOURCES = matcher_util_api_test01.cpp
matcher_util_api_test01_CXXFLAGS = $(AM_CXXFLAGS)
matcher_util_api_test01_LDADD = \
    $(top_builddir)/src/common/libtap/libtap.la \
    $(top_builddir)/resource/libresource.la
