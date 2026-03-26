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
#include <array>
#include <algorithm>

#include "match_op.h"

constexpr auto opt_map = std::to_array<std::pair<match_op_t, const char *>> (
    {{MATCH_UNKNOWN, "error"},
     {MATCH_ALLOCATE, "allocate"},
     {MATCH_ALLOCATE_ORELSE_RESERVE, "allocate_orelse_reserve"},
     {MATCH_ALLOCATE_W_SATISFIABILITY, "allocate_with_satisfiability"},
     {MATCH_SATISFIABILITY, "satisfiability"}});

static_assert (opt_map.size () == END_MATCH_OP_T - START_MATCH_OP_T - 2,
               "opt_map is missing a match_op_t entry");

const char *match_op_to_string (match_op_t match_op)
{
    for (const auto p : opt_map)
        if (p.first == match_op)
            return p.second;

    return "error";
}

match_op_t match_op_from_string (const char *str)
{
    if (!str)
        return MATCH_UNKNOWN;

    for (const auto p : opt_map)
        if (strcmp (p.second, str) == 0)
            return p.first;

    return MATCH_UNKNOWN;
}

bool match_op_valid (match_op_t match_op)
{
    return match_op != MATCH_UNKNOWN
           && count_if (opt_map.begin (), opt_map.end (), [match_op] (auto p) {
                  return p.first == match_op;
              }) == 1;
}
