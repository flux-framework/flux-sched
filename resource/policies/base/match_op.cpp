/*****************************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#include <cstring>
#include <utility>
#include <array>
#include <cerrno>

#include "match_op.h"

constexpr auto opt_map = std::to_array<std::pair<match_op_t, const char *>> (
    {{MATCH_ALLOCATE, "allocate"},
     {MATCH_ALLOCATE_ORELSE_RESERVE, "allocate_orelse_reserve"},
     {MATCH_ALLOCATE_W_SATISFIABILITY, "allocate_with_satisfiability"},
     {MATCH_SATISFIABILITY, "satisfiability"}});

static_assert (opt_map.size () == END_MATCH_OP_T - MATCH_UNKNOWN - 1,
               "opt_map is missing a match_op_t entry");

const char *match_op_to_string (match_op_t match_op)
{
    if (match_op < 0 || match_op >= END_MATCH_OP_T) {
        errno = EINVAL;
        return nullptr;
    }

    for (const auto &[op, op_str] : opt_map)
        if (op == match_op)
            return op_str;

    errno = ENOENT;
    return nullptr;
}

match_op_t match_op_from_string (const char *str)
{
    if (!str) {
        errno = EINVAL;
        return MATCH_UNKNOWN;
    }

    for (const auto &[op, op_str] : opt_map)
        if (strcmp (op_str, str) == 0)
            return op;

    errno = ENOENT;
    return MATCH_UNKNOWN;
}

bool match_op_valid (match_op_t match_op)
{
    bool rc;
    int saved_errno = errno;

    rc = (match_op_to_string (match_op) != nullptr);
    errno = saved_errno;
    return rc;
}
