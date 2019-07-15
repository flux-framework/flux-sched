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
#include "src/common/libschedutil/schedutil.h"
}

#include "qmanager/policies/base/queue_policy_base.hpp"
#include "qmanager/policies/base/queue_policy_base_impl.hpp"
#include "qmanager/policies/queue_policy_factory_impl.hpp"


using namespace Flux;
using namespace Flux::queue_manager;
using namespace Flux::queue_manager::detail;

/******************************************************************************
 *                                                                            *
 *                 Queue Manager Service Module Context                       *
 *                                                                            *
 ******************************************************************************/

struct qmanager_ctx_t {
    flux_t *h;
    ops_context *ops;
    queue_policy_base_t *queue;
};


/******************************************************************************
 *                                                                            *
 *                     Internal Queue Manager APIs                            *
 *                                                                            *
 ******************************************************************************/

extern "C" int jobmanager_hello_cb (flux_t *h, const char *R, void *arg)
{
    return 0;
}

extern "C" void jobmanager_alloc_cb (flux_t *h, const flux_msg_t *msg,
                                     const char *jobspec, void *arg)
{
    uint32_t userid;
    qmanager_ctx_t *ctx = (qmanager_ctx_t *)arg;
    std::shared_ptr<job_t> job = std::make_shared<job_t> ();

    if (flux_msg_get_userid (msg, &userid) < 0)
        return;

    flux_log (h, LOG_INFO, "alloc requested by user (%u).", userid);

    if (schedutil_alloc_request_decode (msg, &job->id, &job->priority,
                                        &job->userid, &job->t_submit) < 0) {
        flux_log_error (h, "%s: schedutil_alloc_request_decode", __FUNCTION__);
        return;
    }
    job->jobspec = jobspec;
    job->msg = flux_msg_copy (msg, true);
    if (ctx->queue->insert (job) < 0) {
        flux_log_error (h, "%s: queue insert", __FUNCTION__);
        return;
    }
    if (ctx->queue->run_sched_loop ((void *)ctx->h, true) < 0) {
        flux_log (ctx->h, LOG_DEBUG,
                  "%s: return code < 0 from schedule loop", __FUNCTION__);
    }
    while ((job = ctx->queue->alloced_pop ()) != nullptr) {
        flux_log (ctx->h, LOG_DEBUG, "jobid (%ju): %s",
                  (intmax_t)job->id, job->schedule.R.c_str ());
        if (schedutil_alloc_respond_R (ctx->h, job->msg,
                                       job->schedule.R.c_str (), NULL) < 0) {
            flux_log_error (ctx->h, "%s: schedutil_alloc_respond_R",
                            __FUNCTION__);
        }
    }
}

extern "C" void jobmanager_free_cb (flux_t *h, const flux_msg_t *msg,
                                    const char *R, void *arg)
{
    uint32_t userid;
    flux_jobid_t id;
    qmanager_ctx_t *ctx = (qmanager_ctx_t *)arg;
    std::shared_ptr<job_t> job;

    if (flux_msg_get_userid (msg, &userid) < 0)
        return;

    flux_log (h, LOG_INFO, "free requested by user (%u).", userid);

    if (schedutil_free_request_decode (msg, &id) < 0) {
        flux_log_error (h, "%s: schedutil_free_request_decode",
                        __FUNCTION__);
        return;
    }
    if ((ctx->queue->remove (id)) < 0)
        flux_log_error (h, "%s: remove job (%ju)", __FUNCTION__, (intmax_t)id);
    if (ctx->queue->run_sched_loop ((void *)ctx->h, true) < 0) {
        // TODO: Need to tighten up anomalous conditions
        // returned with a negative return code
        // (e.g., unsatisfiable jobs).
        flux_log (ctx->h, LOG_DEBUG,
                  "%s: return code < 0 from schedule loop", __FUNCTION__);
    }
    if (schedutil_free_respond (h, msg) < 0) {
        flux_log_error (h, "%s: schedutil_free_respond", __FUNCTION__);
    }
    while ((job = ctx->queue->alloced_pop ()) != nullptr) {
        flux_log (ctx->h, LOG_DEBUG, "jobid (%ju): %s",
                  (intmax_t)job->id, job->schedule.R.c_str ());
        if (schedutil_alloc_respond_R (ctx->h, job->msg,
                                       job->schedule.R.c_str (), NULL) < 0) {
            flux_log_error (ctx->h, "%s: schedutil_alloc_respond_R",
                            __FUNCTION__);
        }
    }
}

static void jobmanager_exception_cb (flux_t *h, flux_jobid_t id,
                                     const char *t, int s, void *a)
{
    return;
}

static qmanager_ctx_t *qmanager_new (flux_t *h)
{
    int queue_depth = 0;
    qmanager_ctx_t *ctx = NULL;

    if (!(ctx = new (std::nothrow) qmanager_ctx_t ())) {
        errno = ENOMEM;
        goto out;
    }
    ctx->h = h;
    if (!(ctx->queue = create_queue_policy ("fcfs", "module"))) {
        flux_log_error (h, "%s: create_queue_policy", __FUNCTION__);
        goto out;
    }
    if (!(ctx->ops = schedutil_ops_register (ctx->h,
                                             jobmanager_alloc_cb,
                                             jobmanager_free_cb,
                                             jobmanager_exception_cb, ctx))) {
        flux_log_error (ctx->h, "%s: schedutil_ops_register", __FUNCTION__);
        goto out;
    }
    if (schedutil_hello (ctx->h, jobmanager_hello_cb, ctx) < 0) {
        flux_log_error (ctx->h, "%s: schedutil_hello", __FUNCTION__);
        goto out;
    }
    if (schedutil_ready (h, "single", &queue_depth)) {
        flux_log_error (h, "%s: schedutil_ready", __FUNCTION__);
        goto out;
    }

out:
    return ctx;
}

static void qmanager_destroy (qmanager_ctx_t *ctx)
{
    if (ctx) {
        int saved_errno = errno;
        std::shared_ptr<job_t> job;
        while ((job = ctx->queue->pending_pop ()) != nullptr)
            flux_respond_error (ctx->h, job->msg, ENOSYS, "unloading");
        while ((job = ctx->queue->complete_pop ()) != nullptr)
            flux_respond_error (ctx->h, job->msg, ENOSYS, "unloading");
        delete ctx->queue;
        ctx->queue = NULL;
        schedutil_ops_unregister (ctx->ops);
        free (ctx);
        errno = saved_errno;
    }
}


/******************************************************************************
 *                                                                            *
 *                               Module Main                                  *
 *                                                                            *
 ******************************************************************************/

extern "C" int mod_main (flux_t *h, int argc, char **argv)
{
    int rc = -1;
    try {
        qmanager_ctx_t *ctx = NULL;
        if (!(ctx = qmanager_new (h)))
            flux_log_error (h, "%s: qmanager_new", __FUNCTION__);
        if ((rc = flux_reactor_run (flux_get_reactor (h), 0)) < 0)
            flux_log_error (h, "%s: flux_reactor_run", __FUNCTION__);
        qmanager_destroy (ctx);
    }
    catch (std::exception &e) {
        flux_log_error (h, "%s: %s", __FUNCTION__, e.what ());
    }
    return rc;
}

MOD_NAME ("qmanager");

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
