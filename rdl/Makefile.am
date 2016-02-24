AM_CFLAGS = @GCCWARN@ $(CODE_COVERAGE_CFLAGS)
AM_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

AM_CPPFLAGS = -I$(top_srcdir) \
    $(JSON_CFLAGS) $(LUA_INCLUDE) $(CZMQ_CFLAGS)

SUBDIRS = . test

fluxluaexecdir = $(luaexecdir)/flux

luamod_ldflags = \
	-avoid-version -module -shared --disable-static \
	-Wl,--no-undefined

dist_lua_SCRIPTS = RDL.lua

nobase_dist_lua_SCRIPTS = \
	middleclass.lua \
	RDL/Resource.lua \
	RDL/ResourceData.lua \
	RDL/lib/ListOf.lua \
	RDL/memstore.lua \
	RDL/serialize.lua \
	RDL/types/Node.lua \
	RDL/types/Socket.lua \
	RDL/uri.lua \
	RDL/uuid.lua

lib_LTLIBRARIES = libflux-rdl.la
fluxluaexec_LTLIBRARIES = flux-cpuset.la

fluxschedinclude_HEADERS = rdl.h

flux_cpuset_la_SOURCES = lua-cpuset.c cpuset-str.c cpuset-str.h
flux_cpuset_la_CFLAGS = $(LUA_INCLUDE)
flux_cpuset_la_LIBADD = $(LUA_LIB)
flux_cpuset_la_LDFLAGS = $(luamod_ldflags)

libflux_rdl_la_SOURCES = rdl.c json-lua.c json-lua.h
libflux_rdl_la_LIBADD = $(FLUX_CORE_LIBS) $(LUA_LIB) $(JSON_LIBS) $(CZMQ_LIBS) \
    $(top_builddir)/src/common/liblsd/liblsd.la \
    $(top_builddir)/src/common/libutil/libutil.la
libflux_rdl_la_LDFLAGS = $(fluxlib_ldflags)

fluxcmd_PROGRAMS = flux-rdltool
flux_rdltool_SOURCES = flux-rdltool.c
flux_rdltool_CFLAGS = $(AM_CFLAGS)
flux_rdltool_LDADD = libflux-rdl.la $(LUA_LIB) $(JSON_LIBS)
