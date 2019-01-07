# ===========================================================================
#      https://github.com/flux-framework/flux-sched/
# ===========================================================================
# SYNOPSIS
#
#   AX_FLUX_CORE
#
# DESCRIPTION
#
#   Find flux-core with pkg-config, setting FLUX_CORE_LIBS, _CFLAGS, and:
#   - FLUX            flux(1) command path
#   - FLUX_PREFIX     flux-core configured prefix
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
  PKG_CHECK_MODULES([FLUX_CORE], [flux-core],
    [ AC_CHECK_PROG(FLUX,[flux],[flux])
      FLUX_PREFIX=`pkg-config --variable=prefix flux-core`
      if test -z "${FLUX_PREFIX}" && test -n "${FLUX}" ; then
        FLUX_PREFIX=`echo ${FLUX} | sed 's/\/bin\/flux//'`
      fi
      AC_SUBST(FLUX_PREFIX)
    ],
    AC_MSG_ERROR([flux-core package not installed]))
  ]
)

