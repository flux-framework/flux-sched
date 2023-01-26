AM_CPPFLAGS = \
	-I$(top_srcdir)

noinst_PROGRAMS = flux-jobspec-validate
noinst_LTLIBRARIES = libjobspec_conv.la

flux_jobspec_validate_SOURCES = flux-jobspec-validate.cpp
flux_jobspec_validate_LDADD = \
    libjobspec_conv.la \
    $(YAMLCPP_LIBS)

libjobspec_conv_la_CXXFLAGS = \
	$(WARNING_CXXFLAGS) \
	$(CODE_COVERAGE_CXXFLAGS) \
	$(YAMLCPP_CFLAGS) \
	$(FLUX_HOSTLIST_CFLAGS) \
	$(FLUX_IDSET_CFLAGS)

libjobspec_conv_la_LIBADD = \
	$(CODE_COVERAGE_LIBS) \
	$(YAMLCPP_LIBS) \
	$(FLUX_HOSTLIST_LIBS) \
	$(FLUX_IDSET_LIBS)

libjobspec_conv_la_SOURCES = \
	jobspec.cpp \
	jobspec.hpp \
	parse_error.hpp \
	constraint.hpp \
	constraint.cpp \
	hostlist_constraint.hpp \
	hostlist_constraint.cpp \
	rank_constraint.hpp \
	rank_constraint.cpp

TESTS = \
	test_constraint.t

check_PROGRAMS = \
	$(TESTS)

test_constraint_t_SOURCES = \
	test/constraint.cpp
test_constraint_t_CXXFLAGS = \
	$(WARNING_CXXFLAGS) \
	$(CODE_COVERAGE_CXXFLAGS) \
	$(YAMLCPP_CFLAGS)
test_constraint_t_LDADD = \
	$(CODE_COVERAGE_LIBS) \
	$(YAMLCPP_LIBS) \
	libjobspec_conv.la \
	$(top_builddir)/src/common/libtap/libtap.la
test_constraint_t_LDFLAGS = \
	-no-install
