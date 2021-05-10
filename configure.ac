#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([flux-sched],
        m4_esyscmd([git describe --always | awk '/.*/ {sub(/^v/, ""); printf "%s",$1; exit}']))
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])
AC_CONFIG_SRCDIR([NEWS])

AM_INIT_AUTOMAKE([subdir-objects tar-ustar filename-length-max=256])
AM_SILENT_RULES([yes])
AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE([enable])

#LT_PREREQ([2.4.2])
LT_INIT([dlopen])

AC_PREFIX_PROGRAM([flux])

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
PKG_CHECK_MODULES([JANSSON], [jansson >= 2.10], [], [])
PKG_CHECK_MODULES([XML2], [libxml-2.0])
AX_VALGRIND_H

PKG_CHECK_MODULES([UUID], [uuid], [], [])
AX_FLUX_CORE

X_AC_YAMLCPP
AX_BOOST_BASE([1.53.0], [], [AC_MSG_ERROR([Please use boost == 1.53 or > 1.58])])
AX_CHECK_BUGGY_BOOST
AX_BOOST_SYSTEM
AX_BOOST_FILESYSTEM
AX_BOOST_GRAPH
AX_BOOST_REGEX
AC_CHECK_LIB([readline], [readline],
             [AC_SUBST([READLINE_LIBS], ["-lreadline"])
              AC_DEFINE([HAVE_LIBREADLINE], [1],
                        [Define if you have libreadline])],
             [AC_MSG_ERROR([Please install GNU readline])])

#  Set PYTHON_VERSION to FLUX_PYTHON_VERSION here
PYTHON_VERSION=${PYTHON_VERSION:-$FLUX_PYTHON_VERSION}
AX_PYTHON_DEVEL([>='3.6'])

AM_PATH_PYTHON([$ac_python_version])
if test "X$PYTHON" = "X"; then
  AC_MSG_ERROR([could not find python])
fi

# Flag for PYTHON_LDFLAGS workaround below.
if test -n "$PYTHON_LDFLAGS"; then
  ac_python_ldflags_set_by_user=true
fi

AM_CHECK_PYMOD(yaml,
               [StrictVersion(yaml.__version__) >= StrictVersion('3.10')], [],
               [AC_MSG_ERROR([[could not find python module yaml, version 3.10+ required]])])

AM_CHECK_PYMOD(jsonschema,
               [StrictVersion(jsonschema.__version__) >= StrictVersion ('2.3.0')],
               ,
               [AC_MSG_ERROR([could not find python module jsonschema, version 2.3.0+ required])]
               )

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

AS_VAR_SET(fluxrc1dir, $sysconfdir/flux/rc1.d)
AC_SUBST(fluxrc1dir)

AS_VAR_SET(fluxrc3dir, $sysconfdir/flux/rc3.d)
AC_SUBST(fluxrc3dir)

AS_VAR_SET(fluxrc1dir, $sysconfdir/flux/rc1.d)
AC_SUBST(fluxrc1dir)

AS_VAR_SET(fluxrc3dir, $sysconfdir/flux/rc3.d)
AC_SUBST(fluxrc3dir)

AS_VAR_SET(fluxionpydir, $pyexecdir/fluxion)
AC_SUBST(fluxionpydir)

# Target of PYTHONPATH set by flux(1) cmddriver, so flux(1)
# doesn't inadvertently insert system python paths (or any
# other python path for that matter) first in PYTHONPATH.
#
AS_VAR_SET(fluxionpylinkdir, $fluxlibdir/python$PYTHON_VERSION)
AC_SUBST(fluxionpylinkdir)

##
# Macros to avoid repetition in Makefiles.am's
##
fluxmod_ldflags="-Wl,--no-undefined -avoid-version -export-symbols-regex '^mod_(main|name|service)\$\$' --disable-static -shared -export-dynamic"
AC_SUBST(fluxmod_ldflags)

fluxplugin_ldflags="-avoid-version -export-symbols-regex '^flux_plugin_init\$\$' --disable-static -shared -export-dynamic"
AC_SUBST(fluxplugin_ldflags)

fluxlib_ldflags="-shared -export-dynamic --disable-static -Wl,--no-undefined"
AC_SUBST(fluxlib_ldflags)

AC_CONFIG_FILES([Makefile
  src/Makefile
  src/common/Makefile
  src/common/libtap/Makefile
  src/common/libutil/Makefile
  src/common/librbtree/Makefile
  src/common/c++wrappers/Makefile
  src/common/c++wrappers/test/Makefile
  src/common/libczmqcontainers/Makefile
  src/cmd/Makefile
  src/python/Makefile
  src/python/fluxion/Makefile
  src/shell/Makefile
  resource/Makefile
  resource/planner/Makefile
  resource/planner/test/Makefile
  resource/evaluators/Makefile
  resource/evaluators/test/Makefile
  resource/libjobspec/Makefile
  resource/utilities/Makefile
  resource/utilities/test/Makefile
  resource/modules/Makefile
  resource/hlapi/Makefile
  resource/hlapi/bindings/Makefile
  resource/hlapi/bindings/c/Makefile
  qmanager/Makefile
  qmanager/modules/Makefile
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
  FLUX_VERSION.....: $($FLUX version | sed -n 's/libflux-core:\t*//p')
  FLUX_CORE_CFLAGS.: $FLUX_CORE_CFLAGS
  FLUX_CORE_LIBS...: $FLUX_CORE_LIBS
  LIBFLUX_VERSION..: $LIBFLUX_VERSION
  FLUX_PREFIX......: $FLUX_PREFIX
  LDFLAGS..........: $LDFLAGS
  LIBS.............: $LIBS
  Linker...........: $LD
"
