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
#include <cstring>
#include <jansson.h>
#include "resource/reapi/bindings/c++/reapi_cli.hpp"
#include "resource/reapi/bindings/c++/reapi_cli_impl.hpp"
#include "resource/schema/data_std.hpp"
#include "resource/reapi/bindings/c/resource_status.h"

using namespace Flux;
using namespace Flux::resource_model;
using namespace Flux::resource_model::detail;

// Conversion helpers between C and C++ status enums
// Returns 0 on success, -1 on error with errno set
static inline int resource_status_to_cpp (resource_status_t status, resource_pool_t::status_t &out)
{
    switch (status) {
        case RESOURCE_UP:
            out = resource_pool_t::status_t::UP;
            return 0;
        case RESOURCE_DOWN:
            out = resource_pool_t::status_t::DOWN;
            return 0;
        default:
            errno = EINVAL;
            return -1;
    }
}

static inline int resource_status_from_cpp (resource_pool_t::status_t status,
                                            resource_status_t &out)
{
    switch (status) {
        case resource_pool_t::status_t::UP:
            out = RESOURCE_UP;
            return 0;
        case resource_pool_t::status_t::DOWN:
            out = RESOURCE_DOWN;
            return 0;
        default:
            errno = EINVAL;
            return -1;
    }
}

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

extern "C" reapi_cli_ctx_t *reapi_cli_clone (reapi_cli_ctx_t *ctx)
{
    if (!ctx || !ctx->rqt) {
        errno = EINVAL;
        return nullptr;
    }

    try {
        auto clone = std::make_unique<reapi_cli_ctx_t> ();
        clone->rqt = new resource_query_t (*ctx->rqt);
        clone->err_msg = "";
        return clone.release ();
    } catch (std::bad_alloc &e) {
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: can't allocate memory: " + std::string (e.what ()) + "\n";
        errno = ENOMEM;
        return nullptr;
    } catch (std::system_error &e) {
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: System error: " + std::string (e.what ()) + "\n";
        errno = e.code ().value ();
        return nullptr;
    } catch (std::runtime_error &e) {
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: Runtime error: " + std::string (e.what ()) + "\n";
        errno = EPROTO;
        return nullptr;
    } catch (...) {
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: unknown exception during clone\n";
        errno = EINVAL;
        return nullptr;
    }
}

extern "C" int reapi_cli_match_with_jobid (reapi_cli_ctx_t *ctx,
                                           match_op_t match_op,
                                           const char *jobspec,
                                           uint64_t jobid,
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

    if ((rc = reapi_cli_t::
             match_allocate (ctx->rqt, match_op, jobspec, jobid, *reserved, R_buf, *at, *ov))
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

    if (!ctx || !ctx->rqt) {
        errno = EINVAL;
        return -1;
    }

    uint64_t seq = ctx->rqt->get_job_counter ();

    if ((rc = reapi_cli_match_with_jobid (ctx, match_op, jobspec, seq, reserved, R, at, ov)) < 0)
        goto done;
    *jobid = seq;

done:
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

    // Not satisfiable if the match reported unsatisfiable (ENODEV) or failed
    // for any other reason (e.g. a jobspec referencing an unknown resource
    // type).  A nonzero return must never be reported as satisfiable.
    if (ret != 0)
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

extern "C" int reapi_cli_add_subgraph (reapi_cli_ctx_t *ctx, const char *R_subgraph)
{
    if (!ctx || !ctx->rqt || !R_subgraph) {
        errno = EINVAL;
        return -1;
    }
    return reapi_cli_t::add_subgraph (ctx->rqt, R_subgraph);
}

extern "C" int reapi_cli_remove_subgraph (reapi_cli_ctx_t *ctx, const char *subgraph_path)
{
    if (!ctx || !ctx->rqt || !subgraph_path) {
        errno = EINVAL;
        return -1;
    }
    return reapi_cli_t::remove_subgraph (ctx->rqt, subgraph_path);
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

extern "C" int reapi_cli_find (reapi_cli_ctx_t *ctx,
                               const char *criteria,
                               const char *format,
                               char **R)
{
    int rc = -1;
    json_t *o = nullptr;
    char *json_str = nullptr;
    std::optional<std::string> format_opt = std::nullopt;

    if (!ctx || !ctx->rqt || !criteria || !R) {
        errno = EINVAL;
        return -1;
    }

    if (format)
        format_opt = std::string (format);

    if ((rc = reapi_cli_t::find (ctx->rqt, criteria, o, format_opt)) < 0) {
        ctx->err_msg = reapi_cli_t::get_err_message ();
        reapi_cli_t::clear_err_message ();
        goto out;
    }

    if (o) {
        if (!(json_str = json_dumps (o, JSON_COMPACT))) {
            ctx->err_msg = __FUNCTION__;
            ctx->err_msg += ": ERROR: can't serialize JSON\n";
            json_decref (o);
            errno = ENOMEM;
            rc = -1;
            goto out;
        }
        json_decref (o);
        *R = json_str;
    } else {
        *R = nullptr;
    }

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

extern "C" const char *reapi_cli_get_err_msg (reapi_cli_ctx_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return strdup ("ERROR: reapi_cli context is null\n");
    }

    std::string err_buf;
    if (ctx->rqt)
        err_buf = ctx->rqt->get_resource_query_err_msg ();
    err_buf += reapi_cli_t::get_err_message () + ctx->err_msg;

    return strdup (err_buf.c_str ());
}

extern "C" void reapi_cli_clear_err_msg (reapi_cli_ctx_t *ctx)
{
    if (ctx->rqt)
        ctx->rqt->clear_resource_query_err_msg ();
    reapi_cli_t::clear_err_message ();
    ctx->err_msg = "";
}

extern "C" int reapi_cli_set_status (reapi_cli_ctx_t *ctx,
                                     const char *resource_path,
                                     resource_status_t status)
{
    if (!ctx || !ctx->rqt || !resource_path) {
        errno = EINVAL;
        return -1;
    }

    resource_pool_t::status_t cpp_status;
    if (resource_status_to_cpp (status, cpp_status) < 0)
        return -1;

    try {
        return reapi_cli_t::set_status (ctx->rqt, resource_path, cpp_status);
    } catch (std::system_error &e) {
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: System error: " + std::string (e.what ()) + "\n";
        errno = e.code ().value ();
        return -1;
    } catch (std::exception &e) {
        // Translate C++ exceptions to errno - unexpected errors default to EINVAL.
        errno = EINVAL;
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: " + std::string (e.what ()) + "\n";
        return -1;
    } catch (...) {
        errno = EINVAL;
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: unknown exception\n";
        return -1;
    }
}

extern "C" int reapi_cli_get_status (reapi_cli_ctx_t *ctx,
                                     const char *resource_path,
                                     resource_status_t *status)
{
    if (!ctx || !ctx->rqt || !resource_path || !status) {
        errno = EINVAL;
        return -1;
    }

    try {
        resource_pool_t::status_t cpp_status;
        int rc = reapi_cli_t::get_status (ctx->rqt, resource_path, cpp_status);
        if (rc == 0 && resource_status_from_cpp (cpp_status, *status) < 0)
            return -1;
        return rc;
    } catch (std::system_error &e) {
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: System error: " + std::string (e.what ()) + "\n";
        errno = e.code ().value ();
        return -1;
    } catch (std::exception &e) {
        // Translate C++ exceptions to errno - unexpected errors default to EINVAL.
        errno = EINVAL;
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: " + std::string (e.what ()) + "\n";
        return -1;
    } catch (...) {
        errno = EINVAL;
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: unknown exception\n";
        return -1;
    }
}

extern "C" int reapi_cli_set_rank_status (reapi_cli_ctx_t *ctx,
                                          const char *ranks,
                                          resource_status_t status)
{
    if (!ctx || !ctx->rqt) {
        errno = EINVAL;
        return -1;
    }

    resource_pool_t::status_t cpp_status;
    if (resource_status_to_cpp (status, cpp_status) < 0)
        return -1;

    try {
        return reapi_cli_t::set_rank_status (ctx->rqt, ranks, cpp_status);
    } catch (std::system_error &e) {
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: System error: " + std::string (e.what ()) + "\n";
        errno = e.code ().value ();
        return -1;
    } catch (std::exception &e) {
        // Translate C++ exceptions to errno - unexpected errors default to EINVAL.
        errno = EINVAL;
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: " + std::string (e.what ()) + "\n";
        return -1;
    } catch (...) {
        errno = EINVAL;
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: unknown exception\n";
        return -1;
    }
}

extern "C" int reapi_cli_get_rank_status (reapi_cli_ctx_t *ctx,
                                          const char *rank,
                                          resource_status_t *status)
{
    if (!ctx || !ctx->rqt || !status) {
        errno = EINVAL;
        return -1;
    }

    try {
        resource_pool_t::status_t cpp_status;
        int rc = reapi_cli_t::get_rank_status (ctx->rqt, rank, cpp_status);
        if (rc == 0 && resource_status_from_cpp (cpp_status, *status) < 0)
            return -1;
        return rc;
    } catch (std::system_error &e) {
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: System error: " + std::string (e.what ()) + "\n";
        errno = e.code ().value ();
        return -1;
    } catch (std::exception &e) {
        // Translate C++ exceptions to errno - unexpected errors default to EINVAL.
        errno = EINVAL;
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: " + std::string (e.what ()) + "\n";
        return -1;
    } catch (...) {
        errno = EINVAL;
        ctx->err_msg = __FUNCTION__;
        ctx->err_msg += ": ERROR: unknown exception\n";
        return -1;
    }
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
