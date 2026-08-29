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
 *    match_within       Only return matches that start between now and now+within.
 *                       If within < 0, don't apply this filter.  However, if
 *                       within == INT64_MIN, also search for a 'within'
 *                       value in the jobspec's user attributes dictionary.
 */
struct traverser_match_attrs {
    double match_overhead;
    int64_t match_within;
};

const struct traverser_match_attrs default_match_attrs = {0.0f, INT64_MIN};

#ifdef __cplusplus
}
#endif

#endif  // DFU_MATCH_ATTRIBUTES_H
