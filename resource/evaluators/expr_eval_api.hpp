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

#ifndef EXPR_EVAL_API_HPP
#define EXPR_EVAL_API_HPP

#include <string>
#include "resource/evaluators/expr_eval_target.hpp"

namespace Flux {
namespace resource_model {

/*! Expression evaluation API class.
 *  Parse and validate or evaluate an expression using
 *  the state of each individual predicates provided
 *  the target state of expr_eval_target_t.
 *  Currently supported expression:
 *  An expression is one or more predicates combined
 *  with logical conjunction ("and"), disjunction ("or"),
 *  or parentheses ("()"). The evaluation order is consistent
 *  with these operators used in predicate calculus.
 */
class expr_eval_api_t {
public:

    /*! Validate if an expression is valid w/ respect to the target state.
     *
     *  \param expr      expression string
     *  \param target    expression evaluation target
     *                       of expr_eval_target_base_t type
     *  \return          0 on success; -1 on error
     */
    int validate (const std::string &expr,
                  const expr_eval_target_base_t &target);

    /*! Evaluate if an expression is valid w/ respect to the target state.
     *
     *  \param expr      expression string
     *  \param target    expression evaluation target
     *                       of expr_eval_target_base_t type
     *  \return          0 on success; -1 on error
     */
    int evaluate (const std::string &expr,
                  const expr_eval_target_base_t &target, bool &result);

private:

    enum class pred_op_t : int {
        AND=0,
        OR=1,
        UNKNOWN=2
    };

    bool is_paren (const std::string &expr, std::size_t at) const;
    size_t find_closing_paren (const std::string &expr, size_t at) const;

    /* Parse methods */
    int parse_expr_leaf (const std::string &expr,
                         size_t at, size_t &tok, size_t &len) const;
    int parse_expr_paren (const std::string &expr,
                          size_t at, size_t &tok, size_t &len) const;
    pred_op_t parse_pred_op (const std::string &e,
                             size_t at, size_t &next) const;

    /* Validate methods */
    int validate_leaf (const std::string &expr,
                       const expr_eval_target_base_t &target);
    int validate_paren (const std::string &expr,
                        const expr_eval_target_base_t &target,
                        size_t at, size_t &nx);
    int validate_pred (pred_op_t op) const;

    /* Evaluate methods */
    int evaluate_leaf (const std::string &expr,
                       const expr_eval_target_base_t &target, bool &result);
    int evaluate_paren (const std::string &expr,
                        const expr_eval_target_base_t &target,
                        size_t at, size_t &next, bool &result);
    int evaluate_pred (pred_op_t op, bool result2, bool &result1) const;

    int m_level = 0;
};

} // namespace resource_model
} // namespace Flux

#endif // EXPR_EVAL_API_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
