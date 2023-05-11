#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([flux-sched],
        m4_esyscmd([git describe --always | awk '/.*/ {sub(/^v/, ""); printf "%s",$1; exit}']))
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])
AC_CONFIG_SRCDIR([NEWS.md])

AM_INIT_AUTOMAKE([subdir-objects tar-ustar filename-length-max=256 foreign])
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
# Check to build with Go
AC_PROG_GO([go])
AC_PATH_PROG(GO, go, nogo)
AS_IF([test "$ac_cv_path_GO" = "nogo"],AC_MSG_NOTICE([could not find go]),[])
AM_CONDITIONAL([WITH_GO], [test "X$WITH_GO" != "X"])
if test "$WITH_GO" = yes; then
  GO_PRINTOUT=$( echo "GO...............: $(go version | sed 's/go version //')" )
fi

if test "$GCC" = yes; then
  WARNING_CFLAGS="-Wall -Werror -Werror=missing-field-initializers -Wno-error=deprecated-declarations"
  AC_SUBST([WARNING_CFLAGS])
fi
if test "$GXX" = "yes"; then
  WARNING_CXXFLAGS=$WARNING_CFLAGS
  AC_SUBST([WARNING_CXXFLAGS])
fi

# Checks for docs.
AC_ARG_ENABLE([docs],
              AS_HELP_STRING([--disable-docs], [disable building docs]))
AS_IF([test "x$enable_docs" != "xno"], [
        PYTHON_VERSION=3
        AM_PATH_PYTHON([3])
        AM_CHECK_PYMOD(sphinx,
                       [StrictVersion(sphinx.__version__) >= StrictVersion ('1.6.7')],
                       [sphinx=true],
                       [sphinx=false; AC_MSG_WARN([could not find sphinx to generate docs, version 1.6.7+ required])]
                       )
        AM_CHECK_PYMOD(docutils,
                       [StrictVersion(docutils.__version__) >= StrictVersion ('0.11.0')],
                       [docutils=true],
                       [docutils=false; AC_MSG_WARN([could not find docutils to generate docs, version 0.11.0+ required])]
                       )
])
#  If --enable-docs=yes, but no doc generator found,
#   then error immediately:
#
AS_IF([test "x$enable_docs" = "xyes" -a "x$sphinx" = "xfalse"],[
  AC_MSG_ERROR([--enable-docs used but no document generator found!])
])
AS_IF([test "x$enable_docs" = "xyes" -a "x$docutils" = "xfalse"],[
  AC_MSG_ERROR([--enable-docs used but docutils not found!])
])
AM_CONDITIONAL([ENABLE_DOCS], [test "x$sphinx" = "xtrue" -a "x$docutils" = "xtrue"])
AC_CHECK_PROG(ASPELL,[aspell],[aspell])


# Checks for libraries.
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([CZMQ], [libczmq >= 3.0.0])
PKG_CHECK_MODULES([HWLOC], [hwloc >= 1.11.1], [], [])
PKG_CHECK_MODULES([JANSSON], [jansson >= 2.10], [], [])
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
PKG_CHECK_MODULES(LIBEDIT, libedit)

#  Set PYTHON_VERSION to FLUX_PYTHON_VERSION here
PYTHON_VERSION=${PYTHON_VERSION:-$FLUX_PYTHON_VERSION}

AM_PATH_PYTHON([$PYTHON_VERSION])
if test "X$PYTHON" = "X"; then
  AC_MSG_ERROR([could not find python])
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
# AC_FUNC_MALLOC
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
  src/common/c++wrappers/Makefile
  src/common/c++wrappers/test/Makefile
  src/cmd/Makefile
  src/python/Makefile
  src/python/fluxion/Makefile
  src/shell/Makefile
  resource/Makefile
  resource/planner/Makefile
  resource/planner/c/Makefile
  resource/planner/c++/Makefile
  resource/planner/test/Makefile
  resource/schema/Makefile
  resource/schema/test/Makefile
  resource/evaluators/Makefile
  resource/evaluators/test/Makefile
  resource/libjobspec/Makefile
  resource/utilities/Makefile
  resource/utilities/test/Makefile
  resource/modules/Makefile
  resource/reapi/Makefile
  resource/reapi/bindings/Makefile
  resource/reapi/bindings/c/Makefile
  resource/reapi/bindings/go/Makefile
  resource/reapi/bindings/go/src/Makefile
  resource/reapi/bindings/go/src/test/Makefile
  resource/policies/Makefile
  resource/policies/base/Makefile
  resource/policies/base/test/Makefile
  qmanager/Makefile
  qmanager/modules/Makefile
  doc/Makefile
  doc/test/Makefile
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
  $GO_PRINTOUT
"
