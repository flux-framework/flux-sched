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
#include "resource/hlapi/bindings/c/reapi_cli.h"
}

#include <cstdlib>
#include <cstdint>
#include <cerrno>
#include "resource/hlapi/bindings/c++/reapi_cli.hpp"
#include "resource/hlapi/bindings/c++/reapi_cli_impl.hpp"

using namespace Flux;
using namespace Flux::resource_model;
using namespace Flux::resource_model::detail;

struct reapi_cli_ctx {
    flux_t *h;
};

extern "C" reapi_cli_ctx_t *reapi_cli_new ()
{
    reapi_cli_ctx_t *ctx = NULL;
    if (!(ctx = (reapi_cli_ctx_t *)malloc (sizeof (*ctx)))) {
        errno = ENOMEM;
        goto out;
    }
    ctx->h = NULL;
out:
    return ctx;
}

extern "C" void reapi_cli_destroy (reapi_cli_ctx_t *ctx)
{
    free (ctx);
}

extern "C" int reapi_cli_match_allocate (reapi_cli_ctx_t *ctx,
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
    if ((rc = reapi_cli_t::match_allocate (ctx->h, orelse_reserve, jobspec,
                                           jobid, *reserved,
                                           R_buf, *at, *ov)) < 0) {
        goto out;
    }
    (*R) = strdup (R_buf.c_str ());

out:
    return rc;
}

extern "C" int reapi_cli_update_allocate (reapi_cli_ctx_t *ctx,
                   const uint64_t jobid, const char *R, int64_t *at,
                   double *ov, const char **R_out)
{
    int rc = -1;
    std::string R_buf = "";
    if (!ctx || !ctx->h || !R) {
        errno = EINVAL;
        goto out;
    }
    if ( (rc = reapi_cli_t::update_allocate (ctx->h,
                                             jobid, R, *at, *ov, R_buf)) < 0) {
        goto out;
    }
    (*R_out) = strdup (R_buf.c_str ());
out:
    return rc;
}

extern "C" int reapi_cli_cancel (reapi_cli_ctx_t *ctx, const uint64_t jobid)
{
    if (!ctx || !ctx->h) {
        errno = EINVAL;
        return -1;
    }
    return reapi_cli_t::cancel (ctx->h, jobid);    
}

extern "C" int reapi_cli_info (reapi_cli_ctx_t *ctx, const uint64_t jobid,
                               bool *reserved, int64_t *at, double *ov)
{
    if (!ctx || !ctx->h) {
        errno = EINVAL;
        return -1;
    }
    return reapi_cli_t::info (ctx->h, jobid, *reserved, *at, *ov);
}

extern "C" int reapi_cli_stat (reapi_cli_ctx_t *ctx, int64_t *V,
                               int64_t *E, int64_t *J, double *load,
                               double *min, double *max, double *avg)
{
    if (!ctx || !ctx->h) {
        errno = EINVAL;
        return -1;
    }
    return reapi_cli_t::stat (ctx->h, *V, *E, *J, *load, *min, *max, *avg);
}

extern "C" int reapi_cli_set_handle (reapi_cli_ctx_t *ctx, void *handle)
{
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    ctx->h = (flux_t *)handle;
    return 0;
}

extern "C" void *reapi_cli_get_handle (reapi_cli_ctx_t *ctx)
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
