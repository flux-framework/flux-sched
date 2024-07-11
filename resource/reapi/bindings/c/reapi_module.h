/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
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
#include "resource/policies/base/match_op.h"

typedef struct reapi_module_ctx reapi_module_ctx_t;

/*! Create and initialize reapi_module context
 */
reapi_module_ctx_t *reapi_module_new ();

/*! Destroy reapi module context
 *
 * \param ctx           reapi_module_ctx_t context object
 */
void reapi_module_destroy (reapi_module_ctx_t *ctx);

/*! Match a jobspec to the "best" resources and either allocate
 *  orelse reserve them. The best resources are determined by
 *  the selected match policy.
 *
 *  \param ctx       reapi_module_ctx_t context object
 *  \param match_op  match_op_t: set to specify the specific match option
 *                   from 1 of 4 choices:
 *                   MATCH_ALLOCATE: try to allocate now and fail if resources
 *                   aren't available.
 *                   MATCH_ALLOCATE_ORELSE_RESERVE : Try to allocate and reserve
 *                   if resources aren't available now.
 *                   MATCH_SATISFIABILITY: Do a satisfiablity check and do not
 *                   allocate.
 *                   MATCH_ALLOCATE_W_SATISFIABILITY: try to allocate and run
 *                   satisfiability check if resources are not available.
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
int reapi_module_match (reapi_module_ctx_t *ctx,
                        match_op_t match_op,
                        const char *jobspec,
                        const uint64_t jobid,
                        bool *reserved,
                        char **R,
                        int64_t *at,
                        double *ov);

/*! Match a jobspec to the "best" resources and either allocate
 *  orelse reserve them. The best resources are determined by
 *  the selected match policy.
 *
 *  \param ctx       reapi_module_ctx_t context object
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
int reapi_module_match_allocate (reapi_module_ctx_t *ctx,
                                 bool orelse_reserve,
                                 const char *jobspec,
                                 const uint64_t jobid,
                                 bool *reserved,
                                 char **R,
                                 int64_t *at,
                                 double *ov);

/*! Run Satisfiability check for jobspec.
 *
 *  \param ctx       reapi_module_ctx_t context object
 *  \param jobspec   jobspec string.
 *  \param ov        Double into which to return performance overhead
 *                   in terms of elapse time needed to complete
 *                   the match operation.
 *  \return          0 on success; -1 on error.
 */
int reapi_module_match_satisfy (reapi_module_ctx_t *ctx, const char *jobspec, double *ov);

/*! Update the resource state with R.
 *
 *  \param ctx       reapi_module_ctx_t context object
 *  \param jobid     jobid of the uint64_t type.
 *  \param R         R string
 *  \param at        return the scheduled time
 *  \param ov        return the performance overhead
 *                   in terms of elapse time needed to complete
 *                   the match operation.
 *  \param R_out     return the updated R string.
 *  \return          0 on success; -1 on error.
 */
int reapi_module_update_allocate (reapi_module_ctx_t *ctx,
                                  const uint64_t jobid,
                                  const char *R,
                                  int64_t *at,
                                  double *ov,
                                  const char **R_out);

/*! Cancel the allocation or reservation corresponding to jobid.
 *
 *  \param ctx       reapi_module_ctx_t context object
 *  \param jobid     jobid of the uint64_t type.
 *  \param noent_ok  don't return an error on nonexistent jobid
 *  \return          0 on success; -1 on error.
 */
int reapi_module_cancel (reapi_module_ctx_t *ctx, const uint64_t jobid, bool noent_ok);

/*! Cancel the allocation or reservation corresponding to jobid.
 *
 *  \param ctx       reapi_module_ctx_t context object
 *  \param jobid     jobid of the uint64_t type.
 *  \param R         R string to remove
 *  \param noent_ok  don't return an error on nonexistent jobid
 *  \param full_removal  don't return an error on nonexistent jobid
 *  \return          0 on success; -1 on error.
 */
int reapi_module_partial_cancel (reapi_module_ctx_t *ctx,
                                 const uint64_t jobid,
                                 const char *R,
                                 bool noent_ok,
                                 bool &full_removal);

/*! Get the information on the allocation or reservation corresponding
 *  to jobid.
 *
 *  \param ctx       reapi_module_ctx_t context object
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
int reapi_module_info (reapi_module_ctx_t *ctx,
                       const uint64_t jobid,
                       bool *reserved,
                       int64_t *at,
                       double *ov);

/*! Get the performance information about the resource infrastructure.
 *
 *  \param ctx       reapi_module_ctx_t context object
 *  \param V         Number of resource vertices
 *  \param E         Number of edges
 *  \param J         Number of jobs
 *  \param load      Graph load time
 *  \param min       Min match time
 *  \param max       Max match time
 *  \param avg       Avg match time
 *  \return          0 on success; -1 on error.
 */
int reapi_module_stat (reapi_module_ctx_t *ctx,
                       int64_t *V,
                       int64_t *E,
                       int64_t *J,
                       double *load,
                       double *min,
                       double *max,
                       double *avg);

/*! Set the opaque handle to the reapi module context.
 *
 *  \param ctx       reapi_module_ctx_t context object
 *  \param h         Opaque handle. How it is used is an implementation
 *                   detail. However, when it is used within a Flux's
 *                   service module, it is expected to be a pointer
 *                   to a flux_t object.
 *  \return          0 on success; -1 on error.
 */
int reapi_module_set_handle (reapi_module_ctx_t *ctx, void *handle);

/*! Set the opaque handle to the reapi module context.
 *
 *  \param ctx       reapi_module_ctx_t context object
 *  \return          handle
 */
void *reapi_module_get_handle (reapi_module_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif  // REAPI_MODULE_H

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
