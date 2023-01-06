AM_CXXFLAGS = \
	$(WARNING_CXXFLAGS) \
	$(CODE_COVERAGE_CXXFLAGS)

AM_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

AM_CPPFLAGS = -I$(top_srcdir) $(CZMQ_CFLAGS) $(FLUX_CORE_CFLAGS)

TESTS_ENVIRONMENT = \
    TESTRESRC_INPUT_FILE="$(abs_top_srcdir)/conf/hype.lua"

TESTS = schema_test01 schema_test02

check_PROGRAMS = $(TESTS)
schema_test01_SOURCES = schema_test01.cpp
schema_test01_CXXFLAGS = \
	$(AM_CXXFLAGS) \
	-I$(top_srcdir)/resource/schema
schema_test01_LDADD = \
	$(top_builddir)/resource/libresource.la \
	$(top_builddir)/src/common/libtap/libtap.la \
	$(CZMQ_LIBS)

schema_test02_SOURCES = schema_test02.cpp
schema_test02_CXXFLAGS = \
	$(AM_CXXFLAGS) \
	-I$(top_srcdir)/resource/schema
schema_test02_LDADD = \
	$(top_builddir)/resource/libresource.la \
	$(top_builddir)/src/common/libtap/libtap.la \
	$(CZMQ_LIBS)
