noinst_PROGRAMS = flux-jobspec-validate
noinst_LTLIBRARIES = libjobspec_conv.la

flux_jobspec_validate_SOURCES = flux-jobspec-validate.cpp
flux_jobspec_validate_LDADD = \
    libjobspec_conv.la \
    $(YAMLCPP_LIBS)

libjobspec_conv_la_CXXFLAGS = \
	$(WARNING_CXXFLAGS) \
	$(CODE_COVERAGE_CXXFLAGS) \
	$(YAMLCPP_CFLAGS)
libjobspec_conv_la_LIBADD = $(CODE_COVERAGE_LIBS) $(YAMLCPP_LIBS)
libjobspec_conv_la_SOURCES = jobspec.cpp jobspec.hpp

