/*****************************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef MATCH_OP_H
#define MATCH_OP_H

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum match_op_t {
    MATCH_UNKNOWN,
    MATCH_ALLOCATE,
    MATCH_ALLOCATE_W_SATISFIABILITY,
    MATCH_ALLOCATE_ORELSE_RESERVE,
    MATCH_SATISFIABILITY,
    END_MATCH_OP_T
} match_op_t;

const char *match_op_to_string (match_op_t match_op);

match_op_t match_op_from_string (const char *str);

bool match_op_valid (match_op_t match_op);

#ifdef __cplusplus
}
#endif

#endif  // MATCH_OP_H
