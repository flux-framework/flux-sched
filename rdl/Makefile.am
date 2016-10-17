AM_CFLAGS = @GCCWARN@ $(CODE_COVERAGE_CFLAGS)
AM_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

AM_CPPFLAGS = -I$(top_srcdir) \
    $(JANSSON_CFLAGS) $(LUA_INCLUDE) $(CZMQ_CFLAGS)

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

noinst_LTLIBRARIES = libjansson-lua.la
lib_LTLIBRARIES = libflux-rdl.la
fluxluaexec_LTLIBRARIES = \
    flux/cpuset.la

fluxschedinclude_HEADERS = rdl.h

flux_cpuset_la_SOURCES = lua-cpuset.c cpuset-str.c cpuset-str.h
flux_cpuset_la_CFLAGS = $(LUA_INCLUDE) $(AM_CFLAGS)
flux_cpuset_la_LIBADD = $(LUA_LIB)
flux_cpuset_la_LDFLAGS = \
    $(luamod_ldflags) \
    -Wl,--defsym=luaopen_flux_cpuset=luaopen_cpuset \
    $(AM_LDFLAGS)


libjansson_lua_la_SOURCES = jansson-lua.c jansson-lua.h
libjansson_lua_la_LIBADD = $(LUA_LIB) $(JANSSON_LIBS)

libflux_rdl_la_SOURCES = rdl.c
libflux_rdl_la_LIBADD = $(FLUX_CORE_LIBS) $(LUA_LIB) $(CZMQ_LIBS) $(JANSSON_LIBS) \
    libjansson-lua.la \
    $(top_builddir)/src/common/liblsd/liblsd.la \
    $(top_builddir)/src/common/libutil/libutil.la
libflux_rdl_la_LDFLAGS = $(fluxlib_ldflags) \
    -Wl,--version-script=$(srcdir)/rdl_version.map \
    $(AM_LDFLAGS)

fluxcmd_PROGRAMS = flux-rdltool
flux_rdltool_SOURCES = flux-rdltool.c
flux_rdltool_CFLAGS = $(AM_CFLAGS)
flux_rdltool_LDADD = libflux-rdl.la $(LUA_LIB) $(JANSSON_LIBS)

EXTRA_DIST = rdl_version.map

# Copy any Lua modules in flux/.libs to ${top_builddir}/flux/*.so so that
#  they can be used as require 'flux.<name>' in-tree
#
.PHONY: convenience-link clean-convenience-link

convenience-link: $(fluxluaexec_LTLIBRARIES)
	@for f in $^; do \
	  soname=`$(GREP) "^dlname=" $$f | $(SED) -e "s|^dlname='\(.*\)'|\1|"`; \
	  dirname=`dirname $(abs_builddir)/$$f `; \
	  target=$$dirname/.libs/$$soname; link=$$dirname/$$soname; \
	  shortdir=`echo $$f | $(SED) -e 's|[^/]*.la||'`; \
	  shorttarget="$${shortdir}.libs/$$soname"; \
	  echo "  LN       $$shortdir$$soname -> $$shorttarget"; \
	  rm -f  $$link; \
	  $(LN_S) $$target $$link; \
	done


clean-convenience-link:
	@for f in $^; do \
	  soname=`$(GREP) "^dlname=" $$f | $(SED) -e "s|^dlname='\(.*\)'|\1|"`; \
	  dirname=`echo $(abs_builddir)/$$f | $(SED) -e 's|/[^/]*.la||'`; \
	  target=$$dirname/.libs/$$soname; link=$$dirname/$$soname; \
	  echo "  RM       $$link"; \
	  rm -f $$link; \
	done

all-local:: convenience-link

clean-local:: clean-convenience-link
