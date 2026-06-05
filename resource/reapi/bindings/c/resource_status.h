/*****************************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef RESOURCE_STATUS_H
#define RESOURCE_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum resource_status_t { RESOURCE_UP = 0, RESOURCE_DOWN = 1 } resource_status_t;

#ifdef __cplusplus
}
#endif

#endif  // RESOURCE_STATUS_H
