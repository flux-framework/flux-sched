AM_CFLAGS = @GCCWARN@

AM_CPPFLAGS = $(JSON_CFLAGS) $(LUA_INCLUDE) $(CZMQ_CFLAGS) \
    -I $(FLUX_BUILDDIR)

SUBDIRS = . test

lib_LTLIBRARIES = cpuset.la librdl.la
noinst_LTLIBRARIES = libforeign.la

include_HEADERS = rdl.h

libforeign_la_SOURCES = \
    $(FLUX_BUILDDIR)/src/bindings/lua/json-lua.c \
    $(FLUX_BUILDDIR)/src/common/liblsd/list.c \
    $(FLUX_BUILDDIR)/src/common/libutil/log.c \
    $(FLUX_BUILDDIR)/src/common/libutil/optparse.c \
    $(FLUX_BUILDDIR)/src/common/libutil/xzmalloc.c
libforeign_la_CFLAGS = $(LUA_INCLUDE)

cpuset_la_SOURCES = lua-cpuset.c cpuset-str.c
cpuset_la_CFLAGS = $(LUA_INCLUDE)
cpuset_la_LDFLAGS = -module -version-info 0:0:0

librdl_la_SOURCES = rdl.c
librdl_la_LIBADD = $(FLUX_CORE_LIBS) $(LUA_LIB) $(JSON_LIBS) $(CZMQ_LIBS) \
    libforeign.la
librdl_la_LDFLAGS = -version-info 0:0:0

bin_PROGRAMS = flux-rdltool
flux_rdltool_SOURCES = flux-rdltool.c
flux_rdltool_CFLAGS = $(AM_CFLAGS)
flux_rdltool_LDADD = librdl.la $(LUA_LIB) $(JSON_LIBS)
