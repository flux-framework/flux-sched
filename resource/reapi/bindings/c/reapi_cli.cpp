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
#include "resource/reapi/bindings/c/reapi_cli.h"
}

#include <cstdlib>
#include <cstdint>
#include <cerrno>
#include "resource/reapi/bindings/c++/reapi_cli.hpp"
#include "resource/reapi/bindings/c++/reapi_cli_impl.hpp"

using namespace Flux;
using namespace Flux::resource_model;
using namespace Flux::resource_model::detail;

struct reapi_cli_ctx {
    resource_query_t *rqt;
    std::string err_msg;
};

extern "C" reapi_cli_ctx_t *reapi_cli_new ()
{
    reapi_cli_ctx_t *ctx = nullptr;

    try {
        ctx = new reapi_cli_ctx_t;
    } catch (const std::bad_alloc &e) {
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: can't allocate memory: " + std::string (e.what ()) + "\n";
        errno = ENOMEM;
        goto out;
    }

    ctx->rqt = nullptr;
    ctx->err_msg = "";

out:
    return ctx;
}

extern "C" void reapi_cli_destroy (reapi_cli_ctx_t *ctx)
{
    int saved_errno = errno;
    if (ctx->rqt)
        delete ctx->rqt;
    delete ctx;
    errno = saved_errno;
}

extern "C" int reapi_cli_initialize (reapi_cli_ctx_t *ctx, const char *rgraph, const char *options)
{
    int rc = -1;
    ctx->rqt = nullptr;

    try {
        ctx->rqt = new resource_query_t (rgraph, options);
    } catch (std::bad_alloc &e) {
        ctx->err_msg += __FUNCTION__;
        ctx->err_msg += ": ERROR: can't allocate memory: " + std::string (e.what ()) + "\n";
        errno = ENOMEM;
        goto out;
    } catch (std::runtime_error &e) {
        ctx->err_msg += __FUNCTION__;
        ctx->err_msg += ": Runtime error: " + std::string (e.what ()) + "\n";
        errno = EPROTO;
        goto out;
    }

    rc = 0;

out:
    return rc;
}

extern "C" int reapi_cli_match (reapi_cli_ctx_t *ctx,
                                match_op_t match_op,
                                const char *jobspec,
                                uint64_t *jobid,
                                bool *reserved,
                                char **R,
                                int64_t *at,
                                double *ov)
{
    int rc = -1;
    std::string R_buf = "";
    char *R_buf_c = nullptr;

    if (!ctx || !ctx->rqt) {
        errno = EINVAL;
        goto out;
    }

    *jobid = ctx->rqt->get_job_counter ();
    if ((rc = reapi_cli_t::
             match_allocate (ctx->rqt, match_op, jobspec, *jobid, *reserved, R_buf, *at, *ov))
        < 0) {
        goto out;
    }

    if (!(R_buf_c = strdup (R_buf.c_str ()))) {
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: can't allocate memory\n";
        errno = ENOMEM;
        rc = -1;
        goto out;
    }
    (*R) = R_buf_c;

out:
    return rc;
}

extern "C" int reapi_cli_match_allocate (reapi_cli_ctx_t *ctx,
                                         bool orelse_reserve,
                                         const char *jobspec,
                                         uint64_t *jobid,
                                         bool *reserved,
                                         char **R,
                                         int64_t *at,
                                         double *ov)
{
    match_op_t match_op =
        orelse_reserve ? match_op_t::MATCH_ALLOCATE_ORELSE_RESERVE : match_op_t::MATCH_ALLOCATE;

    return reapi_cli_match (ctx, match_op, jobspec, jobid, reserved, R, at, ov);
}

extern "C" int reapi_cli_match_satisfy (reapi_cli_ctx_t *ctx,
                                        const char *jobspec,
                                        bool *sat,
                                        double *ov)
{
    match_op_t match_op = match_op_t::MATCH_SATISFIABILITY;
    uint64_t jobid;
    bool reserved;
    char *R;
    int64_t at;
    int ret;
    *sat = true;

    ret = reapi_cli_match (ctx, match_op, jobspec, &jobid, &reserved, &R, &at, ov);

    // check for satisfiability
    if (errno == ENODEV)
        *sat = false;

    return ret;
}

extern "C" int reapi_cli_update_allocate (reapi_cli_ctx_t *ctx,
                                          const uint64_t jobid,
                                          const char *R,
                                          int64_t *at,
                                          double *ov,
                                          const char **R_out)
{
    int rc = -1;
    std::string R_buf = "";
    const char *R_buf_c = NULL;
    if (!ctx || !ctx->rqt || !R) {
        errno = EINVAL;
        goto out;
    }
    if ((rc = reapi_cli_t::update_allocate (ctx->rqt, jobid, R, *at, *ov, R_buf)) < 0) {
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

extern "C" int reapi_cli_cancel (reapi_cli_ctx_t *ctx, const uint64_t jobid, bool noent_ok)
{
    if (!ctx || !ctx->rqt) {
        errno = EINVAL;
        return -1;
    }
    return reapi_cli_t::cancel (ctx->rqt, jobid, noent_ok);
}

extern "C" int reapi_cli_partial_cancel (reapi_cli_ctx_t *ctx,
                                         const uint64_t jobid,
                                         const char *R,
                                         bool noent_ok,
                                         bool *full_removal)
{
    if (!ctx || !ctx->rqt || !R) {
        errno = EINVAL;
        return -1;
    }
    return reapi_cli_t::cancel (ctx->rqt, jobid, R, noent_ok, *full_removal);
}

extern "C" int reapi_cli_info (reapi_cli_ctx_t *ctx,
                               const uint64_t jobid,
                               char **mode,
                               bool *reserved,
                               int64_t *at,
                               double *ov)
{
    int rc = -1;
    std::string mode_buf = "";
    char *mode_buf_c = nullptr;

    if (!ctx || !ctx->rqt) {
        errno = EINVAL;
        return -1;
    }
    if ((rc = reapi_cli_t::info (ctx->rqt, jobid, mode_buf, *reserved, *at, *ov)) < 0)
        goto out;
    if (!(mode_buf_c = strdup (mode_buf.c_str ()))) {
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: can't allocate memory\n";
        errno = ENOMEM;
        rc = -1;
        goto out;
    }

    (*mode) = mode_buf_c;

out:
    return rc;
}

extern "C" int reapi_cli_stat (reapi_cli_ctx_t *ctx,
                               int64_t *V,
                               int64_t *E,
                               int64_t *J,
                               double *load,
                               double *min,
                               double *max,
                               double *avg)
{
    if (!ctx || !ctx->rqt) {
        errno = EINVAL;
        return -1;
    }
    return reapi_cli_t::stat (ctx->rqt, *V, *E, *J, *load, *min, *max, *avg);
}

extern "C" int reapi_cli_find (reapi_cli_ctx_t *ctx, const char *criteria, char **out)
{
    int rc = -1;
    std::string out_buf = "";
    char *out_buf_c = nullptr;

    if (!ctx || !ctx->rqt || !criteria) {
        errno = EINVAL;
        return -1;
    }
    const std::string criteria_str = std::string (criteria);

    if ((rc = reapi_cli_t::find (ctx->rqt, criteria_str, out_buf)) < 0)
        goto done;
    if (!(out_buf_c = strdup (out_buf.c_str ()))) {
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: can't allocate memory\n";
        errno = ENOMEM;
        rc = -1;
        goto done;
    }

    (*out) = out_buf_c;

done:
    return rc;
}

extern "C" const char *reapi_cli_get_err_msg (reapi_cli_ctx_t *ctx)
{
    std::string err_buf = "";

    if (ctx->rqt)
        err_buf = ctx->rqt->get_resource_query_err_msg () + reapi_cli_t::get_err_message ()
                  + ctx->err_msg;
    else
        err_buf = reapi_cli_t::get_err_message () + ctx->err_msg;

    return strdup (err_buf.c_str ());
}

extern "C" void reapi_cli_clear_err_msg (reapi_cli_ctx_t *ctx)
{
    if (ctx->rqt)
        ctx->rqt->clear_resource_query_err_msg ();
    reapi_cli_t::clear_err_message ();
    ctx->err_msg = "";
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
