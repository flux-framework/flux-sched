/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef REAPI_CLI_H
#define REAPI_CLI_H

#ifdef __cplusplus
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>

typedef struct reapi_cli_ctx reapi_cli_ctx_t;

/*! Create and initialize reapi_cli context
 */
reapi_cli_ctx_t *reapi_cli_new ();

/*! Destroy reapi cli context
 *
 * \param ctx           reapi_cli_ctx_t context object
 */
void reapi_cli_destroy (reapi_cli_ctx_t *ctx);

/*! Match a jobspec to the "best" resources and either allocate
 *  orelse reserve them. The best resources are determined by
 *  the selected match policy.
 *
 *  \param ctx       reapi_cli_ctx_t context object
 *  \param orelse_reserve
 *                   Boolean: if false, only allocate; otherwise, first try
 *                   to allocate and if that fails, reserve.
 *  \param jobspec   jobspec string.
 *  \param jobid     jobid of the uint64_t type.
 *  \param reserved  Boolean into which to return true if this job has been
 *                   reserved instead of allocated.
 *  \param R         String into which to return the resource set either
 *                   allocated or reserved.
 *  \param at        If allocated, 0 is returned; if reserved, actual time
 *                   at which the job is reserved.
 *  \param ov        Double into which to return performance overhead
 *                   in terms of elapse time needed to complete
 *                   the match operation.
 *  \return          0 on success; -1 on error.
 */
int reapi_cli_match_allocate (reapi_cli_ctx_t *ctx, bool orelse_reserve,
                              const char *jobspec, const uint64_t jobid,
                              bool *reserved,
                              char **R, int64_t *at, double *ov);

/*! Update the resource state with R.
 *
 *  \param ctx       reapi_cli_ctx_t context object
 *  \param jobid     jobid of the uint64_t type.
 *  \param R         R string
 *  \param at        return the scheduled time
 *  \param ov        return the performance overhead
 *                   in terms of elapse time needed to complete
 *                   the match operation.
 *  \param R_out     return the updated R string.
 *  \return          0 on success; -1 on error.
 */
int reapi_cli_update_allocate (reapi_cli_ctx_t *ctx,
                               const uint64_t jobid, const char *R, int64_t *at,
                               double *ov, const char **R_out);

/*! Cancel the allocation or reservation corresponding to jobid.
 *
 *  \param ctx       reapi_cli_ctx_t context object
 *  \param jobid     jobid of the uint64_t type.
 *  \param noent_ok  don't return an error on nonexistent jobid
 *  \return          0 on success; -1 on error.
 */
int reapi_cli_cancel (reapi_cli_ctx_t *ctx,
                      const uint64_t jobid, bool noent_ok);

/*! Get the information on the allocation or reservation corresponding
 *  to jobid.
 *
 *  \param ctx       reapi_cli_ctx_t context object
 *  \param jobid     const jobid of the uint64_t type.
 *  \param reserved  Boolean into which to return true if this job has been
 *                   reserved instead of allocated.
 *  \param at        If allocated, 0 is returned; if reserved, actual time
 *                   at which the job is reserved.
 *  \param ov        Double into which to return performance overhead
 *                   in terms of elapse time needed to complete
 *                   the match operation.
 *  \return          0 on success; -1 on error.
 */
int reapi_cli_info (reapi_cli_ctx_t *ctx, const uint64_t jobid,
                    bool *reserved, int64_t *at, double *ov);

/*! Get the performance information about the resource infrastructure.
 *
 *  \param ctx       reapi_cli_ctx_t context object
 *  \param V         Number of resource vertices
 *  \param E         Number of edges
 *  \param J         Number of jobs
 *  \param load      Graph load time
 *  \param min       Min match time
 *  \param max       Max match time
 *  \param avg       Avg match time
 *  \return          0 on success; -1 on error.
 */
int reapi_cli_stat (reapi_cli_ctx_t *ctx, int64_t *V, int64_t *E,
                    int64_t *J, double *load,
                    double *min, double *max, double *avg);

/*! Get the reapi cli error message.
 *
 *  \param ctx       reapi_cli_ctx_t context object
 *  \return          string containing the error message
 */
const char *reapi_cli_get_err_msg (reapi_cli_ctx_t *ctx);

/*! Clear the reapi cli error message.
 *
 *  \param ctx       reapi_cli_ctx_t context object
 */
void reapi_cli_clear_err_msg (reapi_cli_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif // REAPI_CLI_H

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
