#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([flux-sched],
        m4_esyscmd([git describe --always | awk '/.*/ {sub(/^v/, ""); printf "%s",$1; exit}']))
AC_CONFIG_SRCDIR([NEWS])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([subdir-objects])
AM_SILENT_RULES([yes])

#LT_PREREQ([2.4.2])
LT_INIT([dlopen])

# Checks for programs.
AC_DEFINE([_GNU_SOURCE], 1,
          [Define _GNU_SOURCE so that we get all necessary prototypes])
AC_PROG_CC_C99
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX([11], [noext], [mandatory])
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_CC_C_O
AX_CODE_COVERAGE

if test "$GCC" = yes; then
  WARNING_CFLAGS="-Wall -Werror -Werror=missing-field-initializers -Wno-error=deprecated-declarations"
  AC_SUBST([WARNING_CFLAGS])
fi
if test "$GXX" = "yes"; then
  WARNING_CXXFLAGS=$WARNING_CFLAGS
  AC_SUBST([WARNING_CXXFLAGS])
fi

# Checks for libraries.
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([CZMQ], [libczmq >= 3.0.0])
AC_CHECK_LIB([dl], [dlerror],
             [AC_SUBST([DL_LIBS], ["-ldl"])
              AC_DEFINE([HAVE_LIBDL], [1],
                        [Define if you have libdl])],
             [AC_MSG_ERROR([Please install dl])])
PKG_CHECK_MODULES([HWLOC], [hwloc >= 1.11.1], [], [])
PKG_CHECK_MODULES([JANSSON], [jansson >= 2.6], [], [])
PKG_CHECK_MODULES([XML2], [libxml-2.0])
AX_VALGRIND_H
AX_PROG_LUA([5.1],[5.3])
AX_LUA_HEADERS
AX_LUA_LIBS
#  Glean liblua name from LUA_LIB set by AX_LUA_LIBS
LIBLUA_SONAME=`echo $LUA_LIB | sed 's/.*-l\(lua[[0-9.-]]*\).*/lib\1.so/'`
AC_MSG_CHECKING([for liblua DSO name])
AS_IF([test "x$LIBLUA_SONAME" != "x"],
      [AC_MSG_RESULT([$LIBLUA_SONAME])],
      [AC_MSG_RESULT([no])
       AC_MSG_ERROR([Unable to determine name of the liblua shared library])
      ])
AC_DEFINE_UNQUOTED([LIBLUA_SO], ["$LIBLUA_SONAME"],
                   [Define the liblua DSO filename])

PKG_CHECK_MODULES([UUID], [uuid], [], [])
AX_FLUX_CORE

AX_BOOST_BASE([1.53.0], [], [AC_MSG_ERROR([Please install boost >= 1.53)])])
AX_CHECK_BUGGY_BOOST
AX_BOOST_SYSTEM
AX_BOOST_FILESYSTEM
AX_BOOST_GRAPH
AX_BOOST_REGEX
PKG_CHECK_MODULES([JOBSPEC], [flux-jobspec], [], [])
PKG_CHECK_MODULES([IDSET], [flux-idset], [], [])
AC_CHECK_LIB([readline], [readline],
             [AC_SUBST([READLINE_LIBS], ["-lreadline"])
              AC_DEFINE([HAVE_LIBREADLINE], [1],
                        [Define if you have libreadline])],
             [AC_MSG_ERROR([Please install GNU readline])])

# Checks for header files.
AC_CHECK_HEADERS([\
  inttypes.h \
  limits.h \
  stdint.h \
  stdlib.h \
  string.h \
  sys/time.h \
  unistd.h \
  readline/readline.h \
  readline/history.h \
])

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

AS_VAR_SET(schedplugindir, $libdir/flux/modules/sched)
AC_SUBST(schedplugindir)

AS_VAR_SET(fluxschedincludedir, $includedir/flux/sched)
AC_SUBST(fluxschedincludedir)

AS_VAR_SET(fluxschedrc1dir, $sysconfdir/flux/rc1.d)
AC_SUBST(fluxschedrc1dir)

AS_VAR_SET(fluxschedrc3dir, $sysconfdir/flux/rc3.d)
AC_SUBST(fluxschedrc3dir)

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
  src/common/librbtree/Makefile
  rdl/Makefile
  rdl/test/Makefile
  resrc/Makefile
  resrc/test/Makefile
  resource/Makefile
  resource/planner/Makefile
  resource/planner/test/Makefile
  resource/modules/Makefile
  sched/Makefile
  sched/priority/Makefile
  sched/priority/modified_fair_tree/Makefile
  simulator/Makefile
  etc/Makefile
  t/Makefile])
AC_OUTPUT

echo "
  $PACKAGE_NAME version $PACKAGE_VERSION
  Prefix...........: $prefix
  Debug Build......: $debug
  C Compiler.......: $CC
  C++ Compiler.....: $CXX
  CFLAGS...........: $CFLAGS
  CPPFLAGS.......... $CPPFLAGS
  CXXFLAGS.......... $CXXFLAGS
  FLUX.............: $FLUX
  FLUX_CORE_CFLAGS.: $FLUX_CORE_CFLAGS
  FLUX_CORE_LIBS...: $FLUX_CORE_LIBS
  FLUX_PREFIX......: $FLUX_PREFIX
  LDFLAGS..........: $LDFLAGS
  LIBS.............: $LIBS
  Linker...........: $LD
"
