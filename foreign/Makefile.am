AM_CFLAGS = @GCCWARN@

AM_CPPFLAGS = $(JSON_CFLAGS) $(LUA_INCLUDE) $(CZMQ_CFLAGS) \
    -I $(FLUX_BUILDDIR)

noinst_LTLIBRARIES = libforeign.la

nodist_libforeign_la_SOURCES = \
    $(FLUX_BUILDDIR)/src/bindings/lua/json-lua.c \
    $(FLUX_BUILDDIR)/src/common/libutil/log.c \
    $(FLUX_BUILDDIR)/src/common/libutil/xzmalloc.c
libforeign_la_LIBADD = $(LUA_LIB)
