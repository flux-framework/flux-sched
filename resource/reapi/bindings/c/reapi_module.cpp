/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
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
#include <flux/core.h>
#include "resource/reapi/bindings/c/reapi_module.h"
}

#include <cstdlib>
#include <cstdint>
#include <cerrno>
#include "resource/reapi/bindings/c++/reapi_module.hpp"
#include "resource/reapi/bindings/c++/reapi_module_impl.hpp"

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
    ctx->h = flux_open (NULL, 0);
out:
    return ctx;
}

extern "C" void reapi_module_destroy (reapi_module_ctx_t *ctx)
{
    free (ctx);
}

extern "C" int reapi_module_match (reapi_module_ctx_t *ctx,
                                   match_op_t match_op,
                                   const char *jobspec,
                                   const uint64_t jobid,
                                   bool *reserved,
                                   char **R,
                                   int64_t *at,
                                   double *ov,
                                   int64_t within)
{
    int rc = -1;
    std::string R_buf = "";
    char *R_buf_c = NULL;

    if (!ctx || !ctx->h) {
        errno = EINVAL;
        goto out;
    }
    if ((rc = reapi_module_t::
             match_allocate (ctx->h, match_op, jobspec, jobid, *reserved, R_buf, *at, *ov, within))
        < 0) {
        goto out;
    }
    if (!(R_buf_c = strdup (R_buf.c_str ()))) {
        rc = -1;
        goto out;
    }
    (*R) = R_buf_c;

out:
    return rc;
}

extern "C" int reapi_module_match_allocate (reapi_module_ctx_t *ctx,
                                            bool orelse_reserve,
                                            const char *jobspec,
                                            const uint64_t jobid,
                                            bool *reserved,
                                            char **R,
                                            int64_t *at,
                                            double *ov)
{
    match_op_t match_op =
        orelse_reserve ? match_op_t::MATCH_ALLOCATE_ORELSE_RESERVE : match_op_t::MATCH_ALLOCATE;

    return reapi_module_match (ctx, match_op, jobspec, jobid, reserved, R, at, ov);
}

extern "C" int reapi_module_match_satisfy (reapi_module_ctx_t *ctx, const char *jobspec, double *ov)
{
    match_op_t match_op = match_op_t::MATCH_SATISFIABILITY;
    const uint64_t jobid = 0;
    bool *reserved;
    char **R;
    int64_t *at;

    return reapi_module_match (ctx, match_op, jobspec, jobid, reserved, R, at, ov);
}

extern "C" int reapi_module_update_allocate (reapi_module_ctx_t *ctx,
                                             const uint64_t jobid,
                                             const char *R,
                                             int64_t *at,
                                             double *ov,
                                             const char **R_out)
{
    int rc = -1;
    std::string R_buf = "";
    const char *R_buf_c = NULL;

    if (!ctx || !ctx->h || !R) {
        errno = EINVAL;
        goto out;
    }
    if ((rc = reapi_module_t::update_allocate (ctx->h, jobid, R, *at, *ov, R_buf)) < 0) {
        goto out;
    }
    if (!(R_buf_c = strdup (R_buf.c_str ()))) {
        rc = -1;
        goto out;
    }
    *R_out = R_buf_c;
out:
    return rc;
}

extern "C" int reapi_module_cancel (reapi_module_ctx_t *ctx, const uint64_t jobid, bool noent_ok)
{
    if (!ctx || !ctx->h) {
        errno = EINVAL;
        return -1;
    }
    return reapi_module_t::cancel (ctx->h, jobid, noent_ok);
}

extern "C" int reapi_module_partial_cancel (reapi_module_ctx_t *ctx,
                                            const uint64_t jobid,
                                            const char *R,
                                            bool noent_ok,
                                            bool &full_removal)
{
    if (!ctx || !ctx->h || !R) {
        errno = EINVAL;
        return -1;
    }
    return reapi_module_t::cancel (ctx->h, jobid, R, noent_ok, full_removal);
}

extern "C" int reapi_module_info (reapi_module_ctx_t *ctx,
                                  const uint64_t jobid,
                                  bool *reserved,
                                  int64_t *at,
                                  double *ov)
{
    if (!ctx || !ctx->h) {
        errno = EINVAL;
        return -1;
    }
    return reapi_module_t::info (ctx->h, jobid, *reserved, *at, *ov);
}

extern "C" int reapi_module_stat (reapi_module_ctx_t *ctx,
                                  int64_t *V,
                                  int64_t *E,
                                  int64_t *J,
                                  double *load,
                                  double *min,
                                  double *max,
                                  double *avg)
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
