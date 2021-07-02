#
# DESCRIPTION
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

m4_define([GOOD_BOOST_VERSION],
          [AC_LANG_PROGRAM([[
#include <boost/version.hpp>
]],[[
(void) ((void)sizeof(char[1 - 2*(!!((BOOST_VERSION) >= ($1)) && !!((BOOST_VERSION) <= ($2)))]));
]])])

AC_DEFUN([AX_CHECK_BUGGY_BOOST], [
    AC_MSG_CHECKING([for buggy boostlib versions (105400-105800)])
    AC_REQUIRE([AC_PROG_CXX])
    AC_LANG_PUSH(C++)
      AC_COMPILE_IFELSE([GOOD_BOOST_VERSION(105400,105800)],[
        AC_MSG_RESULT([no])
      ],[
        AC_MSG_RESULT([yes])
        AC_MSG_ERROR([[Please use Boost::Graph == 1.53 or > 1.58]])
      ])
    AC_LANG_POP([C++])
  ]
)

