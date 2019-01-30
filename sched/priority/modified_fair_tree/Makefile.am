EXTRA_DIST = priority_external_api.hpp modified_fair_tree.hpp

noinst_LTLIBRARIES = libconvenience.la
libconvenience_la_SOURCES = \
	priority_external_api.cpp \
        modified_fair_tree.cpp
libconvenience_la_CXXFLAGS = \
	$(WARNINGS_CXXFLAGS) $(CODE_COVERAGE_CFLAGS) \
	$(FLUX_CORE_CFLAGS) $(JANSSON_CFLAGS) $(CZMQ_CFLAGS) \
	-I$(top_srcdir) -I$(top_srcdir)/resrc
libconvenience_la_LIBADD = $(top_builddir)/resrc/libflux-resrc.la \
	$(FLUX_CORE_LIBS) $(JANSSON_LIBS) $(CZMQ_LIBS)

lib_LTLIBRARIES = priority_mod_fair_tree.la
# Dummy C++ source to cause C++ linking.
nodist_EXTRA_priority_mod_fair_tree_la_SOURCES = dummy.cpp
priority_mod_fair_tree_la_SOURCES =
priority_mod_fair_tree_la_LIBADD = libconvenience.la
priority_mod_fair_tree_la_LDFLAGS = \
	$(CODE_COVERAGE_LIBS) \
	$(fluxlib_ldflags) \
	-avoid-version \
	-module \
	-export-symbols-regex '^(sched_priority_|mod_name).*'

check_PROGRAMS = parse_sacct

parse_sacct_SOURCES = test/parse_sacct.cpp
parse_sacct_CXXFLAGS = \
	$(FLUX_CORE_CFLAGS) $(JANSSON_CFLAGS) $(CZMQ_CFLAGS) \
	-I. -I$(top_srcdir) -I$(top_srcdir)/resrc
parse_sacct_LDADD = libconvenience.la $(FLUX_CORE_LIBS) $(CZMQ_LIBS)
