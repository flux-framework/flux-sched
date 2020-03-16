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

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>
#include "resource/hlapi/bindings/c/reapi_module.h"
}

#include <cstdlib>
#include <cstdint>
#include <cerrno>
#include "resource/hlapi/bindings/c++/reapi_module.hpp"
#include "resource/hlapi/bindings/c++/reapi_module_impl.hpp"

using namespace Flux;
using namespace Flux::resource_model;
using namespace Flux::resource_model::detail;

struct reapi_module_ctx {
    flux_t *h;
};

extern "C" reapi_module_ctx_t *reapi_module_new ()
{
    reapi_module_ctx_t *ctx = NULL;
    if (!(ctx = (reapi_module_ctx_t *)malloc (sizeof (*ctx)))) {
        errno = ENOMEM;
        goto out;
    }
    ctx->h = NULL;
out:
    return ctx;
}

extern "C" void reapi_module_destroy (reapi_module_ctx_t *ctx)
{
    free (ctx);
}

extern "C" int reapi_module_match_allocate (reapi_module_ctx_t *ctx,
                   bool orelse_reserve, const char *jobspec,
                   const uint64_t jobid, bool *reserved,
                   char **R, int64_t *at, double *ov)
{
    int rc = -1;
    std::string R_buf = "";
    if (!ctx || !ctx->h) {
        errno = EINVAL;
        goto out;
    }
    if ((rc = reapi_module_t::match_allocate (ctx->h, orelse_reserve, jobspec,
                                              jobid, *reserved,
                                              R_buf, *at, *ov)) < 0) {
        goto out;
    }
    (*R) = strdup (R_buf.c_str ());

out:
    return rc;
}

extern "C" int reapi_module_update_allocate (reapi_module_ctx_t *ctx,
                                             const uint64_t jobid, const char *R,
                                             int64_t *at, double *ov,
                                             const char **R_out)
{
    int rc = -1;
    std::string R_buf = "";
    const char *R_buf_c = NULL;

    if (!ctx || !ctx->h || !R) {
        errno = EINVAL;
        goto out;
    }
    if ( (rc = reapi_module_t::update_allocate (ctx->h, jobid, R,
                                                *at, *ov, R_buf)) < 0) {
        goto out;
    }
    if ( !(R_buf_c = strdup (R_buf.c_str ()))) {
        rc = -1;
        goto out;
    }
    *R_out = R_buf_c;
out:
    return rc;

}

extern "C" int reapi_module_cancel (reapi_module_ctx_t *ctx, const uint64_t jobid)
{
    if (!ctx || !ctx->h) {
        errno = EINVAL;
        return -1;
    }
    return reapi_module_t::cancel (ctx->h, jobid);    
}

extern "C" int reapi_module_info (reapi_module_ctx_t *ctx, const uint64_t jobid,
                                  bool *reserved, int64_t *at, double *ov)
{
    if (!ctx || !ctx->h) {
        errno = EINVAL;
        return -1;
    }
    return reapi_module_t::info (ctx->h, jobid, *reserved, *at, *ov);
}

extern "C" int reapi_module_stat (reapi_module_ctx_t *ctx, int64_t *V,
                                  int64_t *E, int64_t *J, double *load,
                                  double *min, double *max, double *avg)
{
    if (!ctx || !ctx->h) {
        errno = EINVAL;
        return -1;
    }
    return reapi_module_t::stat (ctx->h, *V, *E, *J, *load, *min, *max, *avg);
}

extern "C" int reapi_module_set_handle (reapi_module_ctx_t *ctx, void *handle)
{
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    ctx->h = (flux_t *)handle;
    return 0;
}

extern "C" void *reapi_module_get_handle (reapi_module_ctx_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return NULL;
    }
    return ctx->h;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
