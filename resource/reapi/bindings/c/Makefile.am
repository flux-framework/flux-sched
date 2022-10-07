AM_CXXFLAGS = \
    $(WARNING_CXXFLAGS) \
    $(CODE_COVERAGE_CXXFLAGS)

AM_CFLAGS = \
    $(WARNING_CFLAGS) \
    $(CODE_COVERAGE_CFLAGS)

AM_CPPFLAGS = \
    -I$(top_srcdir) $(CZMQ_CFLAGS) $(FLUX_CORE_CFLAGS)

AM_LDFLAGS = \
    $(CODE_COVERAGE_LIBS)

noinst_LTLIBRARIES = libreapi_cli.la libreapi_module.la

libreapi_cli_la_SOURCES = \
    reapi_cli.cpp \
    reapi_cli.h \
    $(top_srcdir)/resource/reapi/bindings/c++/reapi.hpp \
    $(top_srcdir)/resource/reapi/bindings/c++/reapi_cli.hpp \
    $(top_srcdir)/resource/reapi/bindings/c++/reapi_cli_impl.hpp

libreapi_module_la_SOURCES = \
    reapi_module.cpp \
    reapi_module.h \
    $(top_srcdir)/resource/reapi/bindings/c++/reapi.hpp \
    $(top_srcdir)/resource/reapi/bindings/c++/reapi_module.hpp \
    $(top_srcdir)/resource/reapi/bindings/c++/reapi_module_impl.hpp
