/*****************************************************************************\
 *  Copyright (c) 2020 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
 *  LLNL-CODE-658032 All rights reserved.
 *
 *  This file is part of the Flux resource manager framework.
 *  For details, see https://github.com/flux-framework.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license, or (at your option)
 *  any later version.
 *
 *  Flux is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *  See also:  http://www.gnu.org/licenses/
\*****************************************************************************/

#ifndef EXPR_EVAL_TARGET_HPP
#define EXPR_EVAL_TARGET_HPP

#include <string>

namespace Flux {
namespace resource_model {


/*! Expression evaluation target base abstract class.
 *  Define the interfaces that expr_eval_api_t uses to evaluate
 *  and validate a leaf-level expression predicate: p(x).
 *  These interfaces are defined as pure virtual methods.
 *  Therefore, a derived class must override and implement them.
 */
class expr_eval_target_base_t {
public:

    /*! Validate if a predicate expression is valid.
     *  As a predicate is often denoted as p(x), the method parameters
     *  follow this convention: i.e., p for predicate name and x
     *  for the input to the predicate.
     *
     *  \param p         predicate name
     *  \param x         input to the predicate 
     *  \return          0 on success; -1 on error
     */
    virtual int validate (const std::string &p, const std::string &x) const = 0;

    /*! Evaluate if predicate p(x) is true or false.
     *  As a predicate is often denoted as p(x), the method parameters
     *  follow this convention: i.e., p for predicate name and x
     *  for the input to the predicate.
     *
     *  \param p         predicate name
     *  \param x         input to the predicate
     *  \param result    return true or false as p(x) is evaluated on
     *                   this expression evaluation target.
     *  \return          0 on success; -1 on error
     */
    virtual int evaluate (const std::string &p,
                          const std::string &x, bool &result) const = 0;
};

} // resource_model
} // Flux

#endif // EXPR_EVAL_TARGET_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
