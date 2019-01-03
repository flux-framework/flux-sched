AM_CFLAGS = $(WARNING_CFLAGS) $(CODE_COVERAGE_CFLAGS)
AM_LDFLAGS = $(CODE_COVERAGE_LIBS)

AM_CPPFLAGS = -I$(top_srcdir) \
    $(JANSSON_CFLAGS) $(LUA_INCLUDE) $(CZMQ_CFLAGS)

SUBDIRS = . test

fluxluaexecdir = $(luaexecdir)/flux

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

noinst_LTLIBRARIES = libflux-rdl.la

libflux_rdl_la_SOURCES = \
    rdl.h \
    rdl.c \
    jansson-lua.h \
    jansson-lua.c
libflux_rdl_la_LIBADD = \
    $(top_builddir)/src/common/liblsd/liblsd.la \
    $(top_builddir)/src/common/libutil/libutil.la \
    $(FLUX_CORE_LIBS) $(LUA_LIB) $(CZMQ_LIBS) $(JANSSON_LIBS)
libflux_rdl_la_LDFLAGS = \
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
