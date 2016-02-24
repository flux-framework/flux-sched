#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([flux-sched],
        m4_esyscmd([git describe --always | awk '/.*/ {printf "%s",$1; exit}']))
AC_CONFIG_SRCDIR([NEWS])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE
AM_SILENT_RULES([yes])

#LT_PREREQ([2.4.2])
LT_INIT([dlopen])

# Checks for programs.
AC_DEFINE([_GNU_SOURCE], 1,
          [Define _GNU_SOURCE so that we get all necessary prototypes])
AC_PROG_CC_C99
AC_PROG_CXX
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_CC_C_O

GCCWARN="-Wall -Werror -Wno-error=deprecated-declarations"
AC_SUBST([GCCWARN])

# Checks for libraries.
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([CZMQ], [libczmq >= 3.0.0])
AC_CHECK_LIB([dl], [dlerror])
AC_CHECK_LIB([hwloc], [hwloc_topology_init])
PKG_CHECK_MODULES([JSON], [json], [],
  [PKG_CHECK_MODULES([JSON], [json-c])])
PKG_CHECK_MODULES([XML2], [libxml-2.0])
AX_PROG_LUA([5.1],[5.3])
AX_LUA_HEADERS
AX_LUA_LIBS
AC_CHECK_LIB([uuid], [uuid_clear])
AX_FLUX_CORE

# Checks for header files.
AC_CHECK_HEADERS([inttypes.h limits.h stdint.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL # in newer ac version:  AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gettimeofday localtime_r memset strchr strdup strerror strstr strtol strtoul])

##
# Project directories
##
AS_VAR_SET(fluxcmddir, $libexecdir/flux/cmd)
AC_SUBST(fluxcmddir)

AS_VAR_SET(fluxlibdir, $libdir/flux)
AC_SUBST(fluxlibdir)

AS_VAR_SET(fluxmoddir, $libdir/flux/modules)
AC_SUBST(fluxmoddir)

AS_VAR_SET(schedplugindir, $libdir/flux/sched-plugin)
AC_SUBST(schedplugindir)

AS_VAR_SET(fluxschedincludedir, $includedir/flux/sched)
AC_SUBST(fluxschedincludedir)


##
# Macros to avoid repetition in Makefiles.am's
##
fluxmod_ldflags="-Wl,--no-undefined -avoid-version -export-symbols-regex '^mod_(main|name|service)\$\$' --disable-static -shared -export-dynamic"
AC_SUBST(fluxmod_ldflags)

fluxlib_ldflags="-shared -export-dynamic --disable-static -Wl,--no-undefined"
AC_SUBST(fluxlib_ldflags)

AC_CONFIG_FILES([Makefile
		 src/Makefile
		 src/common/Makefile
		 src/common/libtap/Makefile
		 src/common/liblsd/Makefile
		 src/common/libutil/Makefile
                 rdl/Makefile
                 rdl/test/Makefile
                 resrc/Makefile
                 resrc/test/Makefile
                 sched/Makefile
                 simulator/Makefile
                 simulator/test/Makefile
                 t/Makefile])
AC_OUTPUT

echo "
  $PACKAGE_NAME version $PACKAGE_VERSION
  Prefix...........: $prefix
  Debug Build......: $debug
  C Compiler.......: $CC
  CFLAGS...........: $CFLAGS
  CPPFLAGS.......... $CPPFLAGS
  FLUX.............: $FLUX
  FLUX_BUILDDIR....: $FLUX_BUILDDIR
  FLUX_CORE_CFLAGS.: $FLUX_CORE_CFLAGS
  FLUX_CORE_LIBS...: $FLUX_CORE_LIBS
  FLUX_PREFIX......: $FLUX_PREFIX
  LDFLAGS..........: $LDFLAGS
  LIBS.............: $LIBS
  Linker...........: $LD
"
