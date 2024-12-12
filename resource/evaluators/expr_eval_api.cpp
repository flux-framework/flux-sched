/*****************************************************************************\
 * Copyright 2020 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

#include "resource/evaluators/expr_eval_api.hpp"

namespace Flux {
namespace resource_model {

////////////////////////////////////////////////////////////////////////////////
// Expression Evaluation API Private Method Definitions
////////////////////////////////////////////////////////////////////////////////

bool expr_eval_api_t::is_paren (const std::string &e, std::size_t at) const
{
    std::size_t fnws = e.find_first_not_of (" \t", at);
    if (fnws == std::string::npos) {
        return false;
    }
    return (e[fnws] == '(');
}

size_t expr_eval_api_t::find_closing_paren (const std::string &e, size_t at) const
{
    int bal;
    std::size_t pa;

    if ((pa = e.find_first_not_of (" \t", at)) == std::string::npos) {
        errno = EINVAL;
        goto done;
    }
    bal = 1;
    pa++;
    while (bal && (pa = e.find_first_of ("()", pa)) != std::string::npos) {
        bal = (e[pa] == ')') ? bal - 1 : bal + 1;
        pa++;
    }
done:
    return pa;
}

int expr_eval_api_t::parse_expr_leaf (const std::string &e, size_t at, size_t &t, size_t &l) const
{
    std::size_t tok;
    std::size_t end;
    if ((tok = e.find_first_not_of (" \t", at)) == std::string::npos) {
        errno = EINVAL;
        return -1;
    }
    if ((end = e.find_first_of (" \t", tok)) == std::string::npos)
        end = e.length ();
    t = tok;
    l = end - tok;
    return 0;
}

int expr_eval_api_t::parse_expr_paren (const std::string &e, size_t at, size_t &t, size_t &l) const
{
    std::size_t tok;
    std::size_t end;
    if ((tok = e.find_first_not_of (" \t", at)) == std::string::npos) {
        errno = EINVAL;
        return -1;
    }
    if (!is_paren (e, tok)) {
        errno = EINVAL;
        return -1;
    }
    if ((end = find_closing_paren (e, tok)) == std::string::npos) {
        errno = EINVAL;
        return -1;
    }
    t = tok;
    l = end - tok;
    return 0;
}

expr_eval_api_t::pred_op_t expr_eval_api_t::parse_pred_op (const std::string &e,
                                                           size_t at,
                                                           size_t &next) const
{
    std::size_t nws;
    std::size_t xws;
    pred_op_t op = pred_op_t::UNKNOWN;

    if (e[at] != ' ' && e[at] != '\t') {
        errno = EINVAL;
        goto done;
    }
    if ((nws = e.find_first_not_of (" \t", at)) == std::string::npos) {
        errno = EINVAL;
        goto done;
    }
    if ((xws = e.find_first_of (" \t", nws)) == std::string::npos)
        xws = e.length ();
    op = pred_op_t::AND;
    next = at;
    if (e.substr (nws, xws - nws) == "and") {
        next = xws;
    } else if (e.substr (nws, xws - nws) == "or") {
        op = pred_op_t::OR;
        next = xws;
    }

done:
    return op;
}

int expr_eval_api_t::validate_leaf (const std::string &e, const expr_eval_target_base_t &target)
{
    int rc = -1;
    std::size_t delim;
    std::string key, value;

    if ((delim = e.find_first_of ("=")) == std::string::npos) {
        errno = EINVAL;
        goto done;
    }
    key = e.substr (0, delim);
    value = e.substr (delim + 1);
    if ((rc = (&target)->validate (key, value)) < 0) {
        errno = EINVAL;
        goto done;
    }

done:
    return rc;
}

int expr_eval_api_t::validate_paren (const std::string &e,
                                     const expr_eval_target_base_t &target,
                                     size_t at,
                                     size_t &nx)
{
    int rc = -1;
    std::size_t tok;
    std::size_t len;

    if (!is_paren (e, at)) {
        if ((rc = parse_expr_leaf (e, at, tok, len)) < 0)
            goto done;
        if ((rc = validate_leaf (e.substr (tok, len), target)) < 0)
            goto done;
    } else {
        if ((rc = parse_expr_paren (e, at, tok, len)) < 0)
            goto done;
        if ((rc = validate (e.substr (tok + 1, len - 2), target)) < 0)
            goto done;
    }
    nx = tok + len;
    rc = 0;
done:
    return rc;
}

int expr_eval_api_t::validate_pred (pred_op_t op) const
{
    int rc = 0;
    switch (op) {
        case pred_op_t::AND:
            break;
        case pred_op_t::OR:
            break;
        default:
            rc = -1;
            errno = EINVAL;
    }
    return rc;
}

int expr_eval_api_t::evaluate_leaf (const std::string &e,
                                    const expr_eval_target_base_t &target,
                                    bool &result)
{
    int rc = -1;
    std::size_t delim;
    std::string key, value;

    if ((delim = e.find_first_of ("=")) == std::string::npos) {
        errno = EINVAL;
        goto done;
    }
    key = e.substr (0, delim);
    value = e.substr (delim + 1);
    if ((rc = (&target)->validate (key, value)) < 0) {
        errno = EINVAL;
        goto done;
    }
    if ((rc = (&target)->evaluate (key, value, result)) < 0) {
        errno = EINVAL;
        goto done;
    }

done:
    return rc;
}

int expr_eval_api_t::evaluate_paren (const std::string &e,
                                     const expr_eval_target_base_t &target,
                                     size_t at,
                                     size_t &nx,
                                     bool &result)
{
    int rc = -1;
    std::size_t tok;
    std::size_t len;

    if (!is_paren (e, at)) {
        if ((rc = parse_expr_leaf (e, at, tok, len)) < 0)
            goto done;
        if ((rc = evaluate_leaf (e.substr (tok, len), target, result)) < 0)
            goto done;
    } else {
        if ((rc = parse_expr_paren (e, at, tok, len)) < 0)
            goto done;
        if ((rc = evaluate (e.substr (tok + 1, len - 2), target, result)) < 0)
            goto done;
    }
    nx = tok + len;
    rc = 0;
done:
    return rc;
}

int expr_eval_api_t::extract_leaf (const std::string &e,
                                   const expr_eval_target_base_t &target,
                                   std::vector<std::pair<std::string, std::string>> &predicates)
{
    int rc = -1;
    std::size_t delim;
    std::string key, value;

    if ((delim = e.find_first_of ("=")) == std::string::npos) {
        errno = EINVAL;
        goto done;
    }
    key = e.substr (0, delim);
    value = e.substr (delim + 1);
    if ((rc = (&target)->extract (key, value, predicates)) < 0) {
        errno = EINVAL;
        goto done;
    }

done:
    return rc;
}

int expr_eval_api_t::evaluate_pred (pred_op_t op, bool result2, bool &result1) const
{
    int rc = 0;
    switch (op) {
        case pred_op_t::AND:
            result1 = result1 && result2;
            break;
        case pred_op_t::OR:
            result1 = result1 || result2;
            break;
        default:
            rc = -1;
            errno = EINVAL;
    }
    return rc;
}

int expr_eval_api_t::extract_paren (const std::string &e,
                                    const expr_eval_target_base_t &target,
                                    size_t at,
                                    size_t &nx,
                                    std::vector<std::pair<std::string, std::string>> &predicates)
{
    int rc = -1;
    std::size_t tok;
    std::size_t len;

    if (!is_paren (e, at)) {
        if ((rc = parse_expr_leaf (e, at, tok, len)) < 0)
            goto done;
        if ((rc = extract_leaf (e.substr (tok, len), target, predicates)) < 0)
            goto done;
    } else {
        if ((rc = parse_expr_paren (e, at, tok, len)) < 0)
            goto done;
        if ((rc = extract (e.substr (tok + 1, len - 2), target, predicates)) < 0)
            goto done;
    }
    nx = tok + len;
    rc = 0;
done:
    return rc;
}

////////////////////////////////////////////////////////////////////////////////
// Expression Evaluation API Public Method Definitions
////////////////////////////////////////////////////////////////////////////////

int expr_eval_api_t::validate (const std::string &e, const expr_eval_target_base_t &target)
{
    int rc = -1;
    pred_op_t op;
    std::size_t next, at;

    if ((rc = validate_paren (e, target, 0, next)) < 0)
        goto done;
    at = next;

    while (at <= e.find_last_not_of (" \t")) {
        if ((op = parse_pred_op (e, at, next)) == pred_op_t::UNKNOWN) {
            rc = -1;
            goto done;
        }
        at = next;
        if ((rc = validate_paren (e, target, at, next)) < 0)
            goto done;
        if ((rc = validate_pred (op)) < 0)
            goto done;
        at = next;
    }
    rc = 0;

done:
    return rc;
}

int expr_eval_api_t::evaluate (const std::string &e,
                               const expr_eval_target_base_t &target,
                               bool &result)
{
    int rc = -1;
    pred_op_t op;
    std::size_t next, at;
    bool result1, result2;

    if ((rc = evaluate_paren (e, target, 0, next, result1)) < 0)
        goto done;
    at = next;

    while (at <= e.find_last_not_of (" \t")) {
        if ((op = parse_pred_op (e, at, next)) == pred_op_t::UNKNOWN) {
            rc = -1;
            goto done;
        }
        at = next;
        if ((rc = evaluate_paren (e, target, at, next, result2)) < 0)
            goto done;
        if ((rc = evaluate_pred (op, result2, result1)) < 0)
            goto done;
        at = next;
    }
    result = result1;
    rc = 0;

done:
    return rc;
}

int expr_eval_api_t::extract (const std::string &e,
                              const expr_eval_target_base_t &target,
                              std::vector<std::pair<std::string, std::string>> &predicates)
{
    int rc = -1;
    pred_op_t op;
    std::size_t next, at, last_before_space;

    if ((rc = extract_paren (e, target, 0, next, predicates)) < 0)
        goto done;
    at = next;

    last_before_space = e.find_last_not_of (" \t");
    while (at <= last_before_space) {
        if ((op = parse_pred_op (e, at, next)) == pred_op_t::UNKNOWN) {
            rc = -1;
            goto done;
        }
        at = next;
        if ((rc = extract_paren (e, target, at, next, predicates)) < 0)
            goto done;
        at = next;
    }
    rc = 0;

done:
    return rc;
}

}  // namespace resource_model
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
