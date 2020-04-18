AM_CXXFLAGS = \
	$(WARNING_CXXFLAGS) \
	$(CODE_COVERAGE_CXXFLAGS)

AM_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

AM_CPPFLAGS = -I$(top_srcdir)

TESTS = exception_safe_wrapper_test01

check_PROGRAMS = $(TESTS)
exception_safe_wrapper_test01_SOURCES = exception_safe_wrapper_test01.cpp
exception_safe_wrapper_test01_CXXFLAGS = \
	$(AM_CXXFLAGS) -I$(top_srcdir)/src/common/c++wrappers
exception_safe_wrapper_test01_LDADD = \
	$(top_builddir)/src/common/libtap/libtap.la

