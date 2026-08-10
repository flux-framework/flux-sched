/*****************************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef DFU_MATCH_ATTRIBUTES_H
#define DFU_MATCH_ATTRIBUTES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*! Wrapper for optional traversal attributes.
 *  Passing around a pointer to traverser_match_attrs allows adding new attributes
 *  without breaking ABI compatibility.
 *    match_overhead     Double to store performance overhead in terms of
 *                       elapsed time needed to complete the match operation.
 *                       Not used by traverser.
 */
struct traverser_match_attrs {
    double match_overhead;
};

const struct traverser_match_attrs default_match_attrs = {0.0f};

#ifdef __cplusplus
}
#endif

#endif  // DFU_MATCH_ATTRIBUTES_H
