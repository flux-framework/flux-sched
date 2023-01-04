AM_CXXFLAGS = \
	$(WARNING_CXXFLAGS) \
	$(CODE_COVERAGE_CXXFLAGS)

AM_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

AM_CPPFLAGS = -I$(top_srcdir) $(CZMQ_CFLAGS) $(FLUX_CORE_CFLAGS)

TESTS_ENVIRONMENT = \
    TESTRESRC_INPUT_FILE="$(abs_top_srcdir)/conf/hype.lua"

TESTS = planner_test01 planner_test02

check_PROGRAMS = $(TESTS)
planner_test01_SOURCES = planner_test01.cpp
planner_test01_CXXFLAGS = $(AM_CXXFLAGS) -I$(top_srcdir)/resource/planner/c -I$(top_srcdir)/resource/planner/c++
planner_test01_LDADD = \
	$(top_builddir)/resource/planner/c/libplanner.la \
	$(top_builddir)/src/common/libtap/libtap.la \
	$(CZMQ_LIBS)

planner_test02_SOURCES = planner_test02.cpp
planner_test02_CXXFLAGS = $(AM_CXXFLAGS) -I$(top_srcdir)/resource/planner/c -I$(top_srcdir)/resource/planner/c++
planner_test02_LDADD = \
	$(top_builddir)/resource/planner/c/libplanner.la \
	$(top_builddir)/src/common/libtap/libtap.la \
	$(CZMQ_LIBS)

