# ===========================================================================
#      https://github.com/flux-framework/flux-sched/
# ===========================================================================
# SYNOPSIS
#
#   AX_FLUX_CORE
#
# DESCRIPTION
#
#   This macro attempts to find the installed flux-core prefix,
#   the flux(1) command, as well as flux-core associated libraries
#   using pkg-config(1).
#
#   The macro prepends PREFIX/lib/pkgconfig to PKG_CONFIG_PATH
#   so that the flux-core in destination --prefix is found by default.
#
#   After the macro runs, the following variables will be defined:
#
#   - FLUX                 flux(1) command path
#   - FLUX_PREFIX          flux-core configured prefix
#   - FLUX_PYTHON          flux-core configured Python version
#   - LIBFLUX_VERSION      libflux-core version
#   - FLUX_CORE_LIBS       libflux-core LIBS
#   - FLUX_CORE_CFLAGS     libflux-core CFLAGS
#   - FLUX_IDSET_LIBS      libflux-idset LIBS
#   - FLUX_IDSET_CFLAGS    libflux-idset CFLAGS
#   - FLUX_OPTPARSE_LIBS   libflux-optparse LIBS
#   - FLUX_OPTPARSE_CFLAGS libflux-optparse CFLAGS
#   - FLUX_HOSTLIST_LIBS   libflux-hostlist LIBS
#   - FLUX_HOSTLIST_CFLAGS libflux-hostlist CFLAGS
#
# LICENSE
#
#   Copyright (c) 2016 Lawrence Livermore National Security, LLC.  Produced at
#   the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
#   LLNL-CODE-658032 All rights reserved.
#
#   This file is part of the Flux resource manager framework.
#   For details, see https://github.com/flux-framework.
#
#   This program is free software; you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the Free
#   Software Foundation; either version 2 of the license, or (at your option)
#   any later version.
#
#   Flux is distributed in the hope that it will be useful, but WITHOUT
#   ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
#   FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program; if not, write to the Free Software Foundation, Inc.,
#   59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
#   See also:  http://www.gnu.org/licenses/
#

AC_DEFUN([AX_FLUX_CORE], [
  # prepend PREFIX/lib/pkgconfig to PKG_CONFIG_PATH.
  # Allows flux-core in the destination PREFIX to be found by pkg-config
  #  by default, assuming in 99% of cases this is what we want.

  saved_PKG_CONFIG_PATH=$PKG_CONFIG_PATH
  PKG_CONFIG_PATH=${prefix}/lib/pkgconfig:${PKG_CONFIG_PATH}
  export PKG_CONFIG_PATH

  PKG_CHECK_MODULES([FLUX_CORE], [flux-core],
    [
      FLUX_PREFIX=`pkg-config --variable=prefix flux-core`
      LIBFLUX_VERSION=`pkg-config --modversion flux-core`
      if test -z "${FLUX_PREFIX}" ; then
        AC_MSG_ERROR([failed to determine flux-core prefix from pkg-config])
      fi
      AC_SUBST(FLUX_PREFIX)
      AC_SUBST(LIBFLUX_VERSION)

      #  Ensure we find the same flux executable as corresponds to
      #  to libflux-core found by pkg-config by puting FLUX_PREFIX/bin
      #  first in AC_PATH_PROG's path
      #
      AC_PATH_PROG(FLUX,[flux],
                   [AC_MSG_ERROR([failed to find flux executable])],
                   [$FLUX_PREFIX/bin:$PATH])

      #  Set FLUX_PYTHON_VERSION to the version of Python used by flux-core
      FLUX_PYTHON_VERSION=$($FLUX python -c 'import sys; print(".".join(map(str, sys.version_info[[:2]])))')
      AC_SUBST(FLUX_PYTHON_VERSION)
    ],
    AC_MSG_ERROR([flux-core package not installed]))
  ]

  #  Check for other flux-core libraries
  PKG_CHECK_MODULES([FLUX_IDSET], [flux-idset], [], [])
  PKG_CHECK_MODULES([FLUX_SCHEDUTIL], [flux-schedutil], [], [])
  PKG_CHECK_MODULES([FLUX_OPTPARSE], [flux-optparse], [], [])
  PKG_CHECK_MODULES([FLUX_HOSTLIST], [flux-hostlist], [], [])

  PKG_CONFIG_PATH=$saved_PKG_CONFIG_PATH
  export PKG_CONFIG_PATH
)

