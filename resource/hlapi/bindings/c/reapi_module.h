/*****************************************************************************\
 *  Copyright (c) 2019 Lawrence Livermore National Security, LLC.  Produced at
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

#ifndef REAPI_MODULE_H
#define REAPI_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>

typedef struct reapi_module_ctx reapi_module_ctx_t;

reapi_module_ctx_t *reapi_module_new ();
void reapi_module_destroy (reapi_module_ctx_t *ctx);
int reapi_module_match_allocate (reapi_module_ctx_t *ctx, bool orelse_reserve,
                                 const char *jobspec, const uint64_t jobid,
                                 bool *reserved,
                                 char **R, int64_t *at, double *ov);
int reapi_module_cancel (reapi_module_ctx_t *ctx, const uint64_t jobid);
int reapi_module_info (reapi_module_ctx_t *ctx, const uint64_t jobid,
                       bool *reserved, int64_t *at, double *ov);
int reapi_module_stat (reapi_module_ctx_t *ctx, int64_t *V, int64_t *E,
                       int64_t *J, double *load,
                       double *min, double *max, double *avg);
int reapi_module_set_handle (reapi_module_ctx_t *ctx, void *handle);
void *reapi_module_get_handle (reapi_module_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif // REAPI_MODULE_H

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
