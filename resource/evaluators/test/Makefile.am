AM_CXXFLAGS = \
	$(WARNING_CXXFLAGS) \
	$(CODE_COVERAGE_CXXFLAGS)

AM_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

AM_CPPFLAGS = -I$(top_srcdir)

TESTS = expr_eval_test01

check_PROGRAMS = $(TESTS)
expr_eval_test01_SOURCES = expr_eval_test01.cpp
expr_eval_test01_CXXFLAGS = $(AM_CXXFLAGS)
expr_eval_test01_LDADD = \
    $(top_builddir)/src/common/libtap/libtap.la \
    $(top_builddir)/resource/libresource.la

