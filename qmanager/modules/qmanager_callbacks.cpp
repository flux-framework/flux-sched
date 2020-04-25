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
}

#include "qmanager/modules/qmanager_callbacks.hpp"
#include "src/common/c++wrappers/eh_wrapper.hpp"

using namespace Flux;
using namespace Flux::queue_manager;
using namespace Flux::queue_manager::detail;
using namespace Flux::cplusplus_wrappers;

int qmanager_cb_t::post_sched_loop (flux_t *h, schedutil_t *schedutil,
                                    std::shared_ptr<queue_policy_base_t> &queue)
{
    int rc = -1;
    std::shared_ptr<job_t> job = nullptr;

    while ((job = queue->alloced_pop ()) != nullptr) {
        if (schedutil_alloc_respond_R (schedutil, job->msg,
                                       job->schedule.R.c_str (), NULL) < 0) {
            flux_log_error (h, "%s: schedutil_alloc_respond_R", __FUNCTION__);
            goto out;
        }
        flux_log (h, LOG_DEBUG, "alloc success (id=%jd)",
                 static_cast<intmax_t> (job->id));
    }
    while ((job = queue->rejected_pop ()) != nullptr) {
        std::string note = "alloc denied due to type=\"" + job->note + "\"";
        if (schedutil_alloc_respond_denied (schedutil,
                                            job->msg,
                                            note.c_str ()) < 0) {
            flux_log_error (h, "%s: schedutil_alloc_respond_denied",
                            __FUNCTION__);
            goto out;
        }
        flux_log (h, LOG_DEBUG, "%s (id=%jd)", note.c_str (),
                 static_cast<intmax_t> (job->id));
    }
    rc = 0;

out:
    return rc;
}

// FIXME: This will be expanded when we implement full scheduler
// resilency schemes: Issue #470.
int qmanager_cb_t::jobmanager_hello_cb (flux_t *h,
                                        flux_jobid_t id, int prio, uint32_t uid,
                                        double ts, const char *R, void *arg)

{
    int rc = -1;
    qmanager_cb_ctx_t *ctx = nullptr;
    ctx = static_cast<qmanager_cb_ctx_t *> (arg);
    std::shared_ptr<job_t> running_job
        = std::make_shared<job_t> (job_state_kind_t::
                                   RUNNING, id, uid, prio, ts, R);

    if (ctx->queue->reconstruct (running_job) < 0) {
        flux_log_error (h, "%s: reconstruct (id=%jd)", __FUNCTION__,
                       static_cast<intmax_t> (id));
        goto out;
    }
    rc = 0;

out:
    return rc;
}

void qmanager_cb_t::jobmanager_alloc_cb (flux_t *h, const flux_msg_t *msg,
                                         const char *jobspec, void *arg)
{
    qmanager_cb_ctx_t *ctx = nullptr;
    ctx = static_cast<qmanager_cb_ctx_t *> (arg);
    std::shared_ptr<job_t> job = std::make_shared<job_t> ();

    if (schedutil_alloc_request_decode (msg, &job->id, &job->priority,
                                        &job->userid, &job->t_submit) < 0) {
        flux_log_error (h, "%s: schedutil_alloc_request_decode", __FUNCTION__);
        return;
    }
    job->jobspec = jobspec;
    job->msg = flux_msg_copy (msg, true);
    if (ctx->queue->insert (job) < 0) {
        flux_log_error (h, "%s: queue insert (id=%jd)", __FUNCTION__,
                       static_cast<intmax_t> (job->id));
        return;
    }
    if (ctx->queue->run_sched_loop (static_cast<void *> (h), true) < 0
        || post_sched_loop (h, ctx->schedutil, ctx->queue) < 0) {
        flux_log_error (h, "%s: schedule loop", __FUNCTION__);
        return;
    }
}

void qmanager_cb_t::jobmanager_free_cb (flux_t *h, const flux_msg_t *msg,
                                        const char *R, void *arg)
{
    flux_jobid_t id;
    qmanager_cb_ctx_t *ctx = nullptr;
    ctx = static_cast<qmanager_cb_ctx_t *> (arg);

    if (schedutil_free_request_decode (msg, &id) < 0) {
        flux_log_error (h, "%s: schedutil_free_request_decode", __FUNCTION__);
        return;
    }
    if ((ctx->queue->remove (id)) < 0) {
        flux_log_error (h, "%s: remove (id=%jd)", __FUNCTION__,
                       static_cast<intmax_t> (id));
        return;
    }
    if (ctx->queue->run_sched_loop ((void *)h, true) < 0) {
        flux_log_error (h, "%s: run_sched_loop", __FUNCTION__);
        return;
    }
    if (schedutil_free_respond (ctx->schedutil, msg) < 0) {
        flux_log_error (h, "%s: schedutil_free_respond", __FUNCTION__);
        return;
    }
    flux_log (h, LOG_DEBUG, "free succeeded (id=%jd)",
             static_cast<intmax_t> (id));
    if (post_sched_loop (h, ctx->schedutil, ctx->queue) < 0) {
        flux_log_error (h, "%s: post_sched_loop", __FUNCTION__);
        return;
    }
}

void qmanager_cb_t::jobmanager_exception_cb (flux_t *h, flux_jobid_t id,
                                             const char *type,
                                             int severity, void *arg)
{
    std::shared_ptr<job_t> job;
    qmanager_cb_ctx_t *ctx = nullptr;
    ctx = static_cast<qmanager_cb_ctx_t *> (arg);

    if (severity > 0 || (job = ctx->queue->lookup (id)) == nullptr
        || !job->is_pending ())
        return;
    if (ctx->queue->remove (id) < 0) {
        flux_log_error (h, "%s: remove job (%jd)", __FUNCTION__,
                       static_cast<intmax_t> (id));
        return;
    }
    std::string note = std::string ("alloc aborted due to exception type=")
                       + type;
    if (schedutil_alloc_respond_denied (ctx->schedutil,
                                        job->msg,
                                        note.c_str ()) < 0) {
        flux_log_error (h, "%s: schedutil_alloc_respond_denied", __FUNCTION__);
        return;
    }
    flux_log (h, LOG_DEBUG, "%s (id=%jd)", note.c_str (),
             static_cast<intmax_t> (id));
}

int qmanager_safe_cb_t::jobmanager_hello_cb (flux_t *h,
                                             flux_jobid_t id, int prio,
                                             uint32_t uid, double ts,
                                             const char *R, void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    int rc = exception_safe_wrapper (qmanager_cb_t::jobmanager_hello_cb,
                                     h, id, prio, uid, ts, R, arg);
    if (exception_safe_wrapper.bad ())
        flux_log_error (h, "%s: %s", __FUNCTION__,
                        exception_safe_wrapper.get_err_message ());
    return rc;
}

void qmanager_safe_cb_t::jobmanager_alloc_cb (flux_t *h, const flux_msg_t *msg,
                                              const char *jobspec, void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (qmanager_cb_t::jobmanager_alloc_cb,
                            h, msg, jobspec, arg);
    if (exception_safe_wrapper.bad ())
        flux_log_error (h, "%s: %s", __FUNCTION__,
                        exception_safe_wrapper.get_err_message ());
}

void qmanager_safe_cb_t::jobmanager_free_cb (flux_t *h, const flux_msg_t *msg,
                                             const char *R, void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (qmanager_cb_t::jobmanager_free_cb, h, msg, R, arg);
    if (exception_safe_wrapper.bad ())
        flux_log_error (h, "%s: %s", __FUNCTION__,
                        exception_safe_wrapper.get_err_message ());
}

void qmanager_safe_cb_t::jobmanager_exception_cb (flux_t *h, flux_jobid_t id,
                                                  const char *type, int severity,
                                                  void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (qmanager_cb_t::jobmanager_exception_cb,
                            h, id, type, severity, arg);
    if (exception_safe_wrapper.bad ())
        flux_log_error (h, "%s: %s", __FUNCTION__,
                        exception_safe_wrapper.get_err_message ());
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
