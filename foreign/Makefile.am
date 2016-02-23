AM_CFLAGS = @GCCWARN@

AM_CPPFLAGS = $(JSON_CFLAGS) $(LUA_INCLUDE) $(CZMQ_CFLAGS) \
    -I $(FLUX_BUILDDIR)

noinst_LTLIBRARIES = libforeign.la

nodist_libforeign_la_SOURCES =
libforeign_la_LIBADD = $(LUA_LIB)
