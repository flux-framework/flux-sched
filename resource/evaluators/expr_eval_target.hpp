/*****************************************************************************\
 * Copyright 2020 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef EXPR_EVAL_TARGET_HPP
#define EXPR_EVAL_TARGET_HPP

#include <string>
#include <vector>

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
    /*! Validate a predicate expression.
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
    virtual int evaluate (const std::string &p, const std::string &x, bool &result) const = 0;

    /*! Extract jobid and agfilter bool if provided.
     *
     *  \param expr      expression string
     *  \param target    expression evaluation target
     *  \param jobid     jobid optionally specified in expression
     *  \param agfilter  bool optionally specified in expression
     *                   of expr_eval_target_base_t type
     *  \return          0 on success; -1 on error
     */
    virtual int extract (const std::string &p,
                         const std::string &x,
                         std::vector<std::pair<std::string, std::string>> &predicates) const = 0;
};

}  // namespace resource_model
}  // namespace Flux

#endif  // EXPR_EVAL_TARGET_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
