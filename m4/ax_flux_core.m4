# ===========================================================================
#      https://github.com/flux-framework/flux-sched/
# ===========================================================================
# SYNOPSIS
#
#   AX_FLUX_CORE
#
# DESCRIPTION
#
#   Use the specified path to flux-core if it has been given.
#   Otherwise, look to the installed locations.
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
  AC_ARG_WITH([flux-core-builddir],
    [AS_HELP_STRING([--with-flux-core-builddir=[path_to_src]],
      [specify location of installed flux-core build directory])
    ],
    [PKG_CHECK_MODULES([FLUX_CORE], [flux-core],
      [FLUX_BUILDDIR=${withval}
       AC_SUBST(FLUX_BUILDDIR)
       AC_CHECK_PROG(FLUX,[flux],[flux])
       FLUX_PREFIX=`pkg-config --variable=prefix flux-core`
       if test -z "${FLUX_PREFIX}" && test -n "${FLUX}" ; then
         FLUX_PREFIX=`echo ${FLUX} | sed 's/\/bin\/flux//'`
       fi
       AC_SUBST(FLUX_PREFIX)
      ],
      AC_MSG_ERROR([flux-core package not installed]))
    ]
  )
  if test -z "${FLUX_BUILDDIR}" ; then
    AC_MSG_ERROR([you must specify --with-flux-core-builddir])
  fi
  ]
)

