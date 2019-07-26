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

struct qmanager_args_t {
    std::string queue_policy;
    std::string queue_params;
    std::string policy_params;
};

struct qmanager_ctx_t {
    flux_t *h;
    qmanager_args_t args;
    ops_context *ops;
    queue_policy_base_t *queue;
};


/******************************************************************************
 *                                                                            *
 *                     Internal Queue Manager APIs                            *
 *                                                                            *
 ******************************************************************************/

static int post_sched_loop (qmanager_ctx_t *ctx)
{
    int rc = -1;
    std::shared_ptr<job_t> job = nullptr;

    while ((job = ctx->queue->alloced_pop ()) != nullptr) {
        if (schedutil_alloc_respond_R (ctx->h, job->msg,
                                       job->schedule.R.c_str (), NULL) < 0) {
            flux_log_error (ctx->h, "%s: schedutil_alloc_respond_R",
                            __FUNCTION__);
            goto out;
        }
        flux_log (ctx->h, LOG_DEBUG,
                  "alloc success (id=%jd)", (intmax_t)job->id);
    }
    while ((job = ctx->queue->rejected_pop ()) != nullptr) {
        std::string note = "alloc denied due to type=\"" + job->note + "\"";
        if (schedutil_alloc_respond_denied (ctx->h, job->msg, note.c_str ()) < 0) {
            flux_log_error (ctx->h, "%s: schedutil_alloc_respond_denied",
                            __FUNCTION__);
            goto out;
        }
        flux_log (ctx->h, LOG_DEBUG,
                  "%s (id=%jd)", note.c_str (), (intmax_t)job->id);
    }
    rc = 0;

out:
    return rc;
}

// FIXME: This will be expanded when we implement full scheduler
// resilency schemes: Issue #470.
extern "C" int jobmanager_hello_cb (flux_t *h,
                                    flux_jobid_t id, int prio, uint32_t uid,
                                    double ts, const char *R, void *arg)

{
    int rc = -1;
    qmanager_ctx_t *ctx = (qmanager_ctx_t *)arg;
    std::shared_ptr<job_t> running_job
        = std::make_shared<job_t> (job_state_kind_t::
                                   RUNNING, id, uid, prio, ts, R);

    if (ctx->queue->reconstruct (running_job) < 0) {
        flux_log_error (h, "%s: reconstruct (id=%jd)", __FUNCTION__, (intmax_t)id);
        goto out;
    }
    rc = 0;

out:
    return rc;
}

extern "C" void jobmanager_alloc_cb (flux_t *h, const flux_msg_t *msg,
                                     const char *jobspec, void *arg)
{
    qmanager_ctx_t *ctx = (qmanager_ctx_t *)arg;
    std::shared_ptr<job_t> job = std::make_shared<job_t> ();

    if (schedutil_alloc_request_decode (msg, &job->id, &job->priority,
                                        &job->userid, &job->t_submit) < 0) {
        flux_log_error (h, "%s: schedutil_alloc_request_decode", __FUNCTION__);
        return;
    }
    job->jobspec = jobspec;
    job->msg = flux_msg_copy (msg, true);
    if (ctx->queue->insert (job) < 0) {
        flux_log_error (h, "%s: queue insert (id=%jd)",
                        __FUNCTION__, (intmax_t)job->id);
        return;
    }
    if (ctx->queue->run_sched_loop ((void *)ctx->h, true) < 0
        || post_sched_loop (ctx) < 0) {
        flux_log_error (ctx->h, "%s: schedule loop", __FUNCTION__);
        return;
    }
}

extern "C" void jobmanager_free_cb (flux_t *h, const flux_msg_t *msg,
                                    const char *R, void *arg)
{
    flux_jobid_t id;
    qmanager_ctx_t *ctx = (qmanager_ctx_t *)arg;

    if (schedutil_free_request_decode (msg, &id) < 0) {
        flux_log_error (h, "%s: schedutil_free_request_decode", __FUNCTION__);
        return;
    }
    if ((ctx->queue->remove (id)) < 0) {
        flux_log_error (h, "%s: remove (id=%jd)", __FUNCTION__, (intmax_t)id);
        return;
    }
    if (ctx->queue->run_sched_loop ((void *)ctx->h, true) < 0) {
        flux_log_error (ctx->h, "%s: run_sched_loop", __FUNCTION__);
        return;
    }
    if (schedutil_free_respond (h, msg) < 0) {
        flux_log_error (h, "%s: schedutil_free_respond", __FUNCTION__);
        return;
    }
    flux_log (ctx->h, LOG_DEBUG, "free succeeded (id=%jd)", (intmax_t)id);
    if (post_sched_loop (ctx) < 0) {
        flux_log_error (ctx->h, "%s: post_sched_loop", __FUNCTION__);
        return;
    }
}

static void jobmanager_exception_cb (flux_t *h, flux_jobid_t id,
                                     const char *t, int s, void *a)
{
    std::shared_ptr<job_t> job;
    qmanager_ctx_t *ctx = (qmanager_ctx_t *)a;

    if (s > 0 || (job = ctx->queue->lookup (id)) == nullptr
        || !job->is_pending ())
        return;
    if (ctx->queue->remove (id) < 0) {
        flux_log_error (h, "%s: remove job (%jd)", __FUNCTION__, (intmax_t)id);
        return;
    }
    std::string note = std::string ("alloc aborted due to exception type=") + t;
    if (schedutil_alloc_respond_denied (h, job->msg, note.c_str ()) < 0) {
        flux_log_error (h, "%s: schedutil_alloc_respond_denied", __FUNCTION__);
        return;
    }
    flux_log (h, LOG_DEBUG, "%s (id=%jd)", note.c_str (), (intmax_t)id);
}

static int process_args (qmanager_ctx_t *ctx, int argc, char **argv)
{
    int rc = 0;
    qmanager_args_t &args = ctx->args;
    std::string dflt = "";

    for (int i = 0; i < argc; i++) {
        if (!strncmp ("queue-policy=", argv[i], sizeof ("queue-policy"))) {
            dflt = args.queue_policy;
            args.queue_policy = strstr (argv[i], "=") + 1;
            if (!known_queue_policy (args.queue_policy)) {
                flux_log (ctx->h, LOG_ERR,
                          "Unknown queuing policy (%s)! Use default (%s).",
                           args.queue_policy.c_str (), dflt.c_str ());
                args.queue_policy = dflt;
            }
        }
        else if (!strncmp ("queue-params=", argv[i], sizeof ("queue-params"))) {
            args.queue_params = strstr (argv[i], "=") + 1;
        }
        else if (!strncmp ("policy-params=", argv[i],
                               sizeof ("policy-params"))) {
            args.policy_params = strstr (argv[i], "=") + 1;
        }
    }

    return rc;
}

static void set_default_args (qmanager_args_t &args)
{
    args.queue_policy = "fcfs";
    args.policy_params = "";
}

static int handshake_jobmanager (qmanager_ctx_t *ctx)
{
    int rc = -1;
    int queue_depth = 0;  /* Not implemented in job-manager */
    const char *mode = (ctx->args.queue_policy == "fcfs")? "single"
                                                         : "unlimited";
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
    if (schedutil_ready (ctx->h, mode, &queue_depth)) {
        flux_log_error (ctx->h, "%s: schedutil_ready", __FUNCTION__);
        goto out;
    }
    rc = 0;
out:
    return rc;
}

static int enforce_queue_policy (qmanager_ctx_t *ctx)
{
    int rc = -1;
    ctx->queue = create_queue_policy (ctx->args.queue_policy, "module");
    if (!ctx->queue) {
        flux_log_error (ctx->h, "%s: create_queue_policy", __FUNCTION__);
        goto out;
    }
    if (ctx->args.policy_params != ""
        && ctx->queue->set_params (ctx->args.policy_params) < 0) {
        flux_log_error (ctx->h, "%s: queue->set_params", __FUNCTION__);
        goto out;
    }
    if (ctx->args.queue_params != ""
        && ctx->queue->set_params (ctx->args.queue_params) < 0) {
        flux_log_error (ctx->h, "%s: queue->set_params", __FUNCTION__);
        goto out;
    }
    if (ctx->queue->apply_params () < 0) {
        flux_log_error (ctx->h, "%s: queue->apply_params", __FUNCTION__);
        goto out;
    }
    if (handshake_jobmanager (ctx) < 0) {
        flux_log_error (ctx->h, "%s: handshake_jobmanager", __FUNCTION__);
        goto out;
    }
    rc = 0;
out:
    return rc;
}

static qmanager_ctx_t *qmanager_new (flux_t *h)
{
    qmanager_ctx_t *ctx = NULL;

    if (!(ctx = new (std::nothrow) qmanager_ctx_t ())) {
        errno = ENOMEM;
        goto out;
    }
    ctx->h = h;
    set_default_args (ctx->args);

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
        if (!(ctx = qmanager_new (h))) {
            flux_log_error (h, "%s: qmanager_new", __FUNCTION__);
            return rc;
        }
        if ((rc = process_args (ctx, argc, argv)) < 0) {
            flux_log_error (h, "%s: load line argument parsing", __FUNCTION__);
            qmanager_destroy (ctx);
            return rc;
        }
        if ((rc = enforce_queue_policy (ctx)) < 0) {
            flux_log_error (h, "%s: enforce_queue_policy", __FUNCTION__);
            qmanager_destroy (ctx);
            return rc;
        }
        if ((rc = process_args (ctx, argc, argv)) < 0)
            flux_log_error (h, "%s: load line argument parsing", __FUNCTION__);
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
