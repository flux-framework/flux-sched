# ===========================================================================
#      https://github.com/flux-framework/flux-sched/
# ===========================================================================
# SYNOPSIS
#
#   AX_VALGRIND_H
#
# DESCRIPTION
#
#   Find valgrind.h first trying pkg-config, fallback to valgrind.h
#    or valgrind/valgrind.h
#
#  This macro will set
#  HAVE_VALGRIND if valgrind.h support was found
#  HAVE_VALGRIND_H if #include <valgrind.h> works
#  HAVE_VALGRIND_VALGRIND_H if #include <valgrind/valgrind.h> works
#
# LICENSE
#
#   Copyright 2016 Lawrence Livermore National Security, LLC
#   (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
#    
#   This file is part of the Flux resource manager framework.
#   For details, see https://github.com/flux-framework.
#   
#   SPDX-License-Identifier: LGPL-3.0
#

AC_DEFUN([AX_VALGRIND_H], [
  PKG_CHECK_MODULES([VALGRIND], [valgrind],
      [AC_DEFINE([HAVE_VALGRIND], [1], [Define if you have valgrind.h])
       ax_valgrind_saved_CFLAGS=$CFLAGS
       ax_valgrind_saved_CPPFLAGS=$CPPFLAGS
       CFLAGS="$CFLAGS $VALGRIND_CFLAGS"
       CPPFLAGS="$CFLAGS"
       AC_CHECK_HEADERS([valgrind.h valgrind/valgrind.h])
       CFLAGS="$ax_valgrind_saved_CFLAGS"
       CPPFLAGS="$ax_valgrind_saved_CPPFLAGS"
      ],
      [AC_CHECK_HEADERS([valgrind.h valgrind/valgrind.h],
                        [AC_DEFINE([HAVE_VALGRIND], [1],
                                   [Define if you have valgrind.h])])
      ])
  ]
)
