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
#include <jansson.h>
}

#include "qmanager/modules/qmanager_callbacks.hpp"
#include "resource/libjobspec/jobspec.hpp"
#include "src/common/c++wrappers/eh_wrapper.hpp"

using namespace Flux;
using namespace Flux::Jobspec;
using namespace Flux::queue_manager;
using namespace Flux::queue_manager::detail;
using namespace Flux::opts_manager;
using namespace Flux::cplusplus_wrappers;


int qmanager_cb_ctx_t::find_queue (flux_jobid_t id,
                                   std::string &queue_name,
                                   std::shared_ptr<queue_policy_base_t> &queue)
{
    for (auto &kv : queues) {
        if (kv.second->lookup (id) != nullptr) {
            queue_name = kv.first;
            queue = kv.second;
            return 0;
        }
    }
    errno = ENOENT;
    return -1;
}

int qmanager_cb_t::post_sched_loop (flux_t *h, schedutil_t *schedutil,
                                    std::map<std::string,
                                             std::shared_ptr<
                                                 queue_policy_base_t>> &queues)
{
    int rc = -1;
    std::shared_ptr<job_t> job = nullptr;
    for (auto& kv: queues) {
        const std::string &queue_name = kv.first;
        std::shared_ptr<queue_policy_base_t> &queue = kv.second;
        while ( (job = queue->alloced_pop ()) != nullptr) {
            if (schedutil_alloc_respond_success_pack (schedutil, job->msg,
                                                      job->schedule.R.c_str (),
                                                      "{ s:{s:s s:f} }",
                                                      "sched",
                                                          "queue", queue_name.c_str (),
                                                          "t_estimate", 0.0) < 0) {
                flux_log_error (h, "%s: schedutil_alloc_respond_pack (queue=%s)",
                                __FUNCTION__, queue_name.c_str ());
                goto out;
            }
            flux_log (h, LOG_DEBUG, "alloc success (queue=%s id=%jd)",
                     queue_name.c_str (), static_cast<intmax_t> (job->id));
        }
        while ( (job = queue->rejected_pop ()) != nullptr) {
            std::string note = "alloc denied due to type=\"" + job->note + "\"";
            if (schedutil_alloc_respond_deny (schedutil,
                                              job->msg,
                                              note.c_str ()) < 0) {
                flux_log_error (h,
                                "%s: schedutil_alloc_respond_deny (queue=%s)",
                                __FUNCTION__, queue_name.c_str ());
                goto out;
            }
            flux_log (h, LOG_DEBUG, "%s (id=%jd queue=%s)", note.c_str (),
                     static_cast<intmax_t> (job->id), queue_name.c_str ());
        }
        for (job = queue->pending_begin (); job != nullptr;
             job = queue->pending_next ()) {
            // if old_at == at, then no reason to send this annotation again.
            if (job->schedule.at == job->schedule.old_at)
                continue;
            if (schedutil_alloc_respond_annotate_pack (
                    schedutil, job->msg,
                    "{ s:{s:s s:f} }",
                    "sched",
                        "queue", queue_name.c_str (),
                        "t_estimate", static_cast<double> (job->schedule.at))) {
                flux_log_error (h, "%s: schedutil_alloc_respond_annotate_pack",
                                __FUNCTION__);
                goto out;
            }
        }
    }
    rc = 0;

out:
    return rc;
}

int qmanager_cb_t::jobmanager_hello_cb (flux_t *h,
                                        flux_jobid_t id, int prio, uint32_t uid,
                                        double ts, const char *R, void *arg)

{
    int rc = 0;
    json_t *o = NULL;
    json_error_t err;
    std::string R_out;
    char *qn_attr = NULL;
    std::string queue_name;
    std::shared_ptr<queue_policy_base_t> queue;
    std::shared_ptr<job_t> running_job = nullptr;
    qmanager_cb_ctx_t *ctx = static_cast<qmanager_cb_ctx_t *> (arg);

    if ( (o = json_loads (R, 0, &err)) == NULL) {
        rc = -1;
        errno = EPROTO;
        flux_log (h, LOG_ERR, "%s: parsing R for job (id=%jd): %s %s@%d:%d",
                  __FUNCTION__, static_cast<intmax_t> (id),
                  err.text, err.source, err.line, err.column);
        goto out;
    }
    if ( (rc = json_unpack (o, "{ s?:{s?:{s?:{s?:s}}} }",
                                   "attributes",
                                       "system",
                                           "scheduler",
                                                "queue", &qn_attr)) < 0) {
        json_decref (o);
        errno = EPROTO;
        flux_log (h, LOG_ERR, "%s: json_unpack for attributes", __FUNCTION__);
        goto out;
    }

    queue_name = qn_attr? qn_attr : ctx->opts.get_opt ().get_default_queue ();
    json_decref (o);
    queue = ctx->queues.at (queue_name);
    // Note that RFC27 defines 31 as the max priority. Because our queue policy
    // layer sorts the pending jobs in lexicographical order
    // (<priority, t_submit, ...> and lower the better, we adjust priority.
    running_job = std::make_shared<job_t> (job_state_kind_t::RUNNING,
                                                   id, uid, 31 - prio, ts, R);

    if ( (rc = queue->reconstruct (static_cast<void *> (h),
                                   running_job, R_out)) < 0) {
        flux_log_error (h, "%s: reconstruct (id=%jd queue=%s)", __FUNCTION__,
                       static_cast<intmax_t> (id), queue_name.c_str ());
        goto out;
    }
    flux_log (h, LOG_DEBUG, "requeue success (queue=%s id=%jd)",
              queue_name.c_str (), static_cast<intmax_t> (id));

out:
    return rc;
}

void qmanager_cb_t::jobmanager_alloc_cb (flux_t *h, const flux_msg_t *msg,
                                         const char *jobspec, void *arg)
{
    qmanager_cb_ctx_t *ctx = nullptr;
    ctx = static_cast<qmanager_cb_ctx_t *> (arg);
    Flux::Jobspec::Jobspec jobspec_obj{jobspec};
    std::string queue_name = ctx->opts.get_opt ().get_default_queue ();
    std::shared_ptr<job_t> job = std::make_shared<job_t> ();

    if (jobspec_obj.attributes.system.queue != "")
        queue_name = jobspec_obj.attributes.system.queue;
    if (schedutil_alloc_request_decode (msg, &job->id, &job->priority,
                                        &job->userid, &job->t_submit) < 0) {
        flux_log_error (h, "%s: schedutil_alloc_request_decode", __FUNCTION__);
        return;
    }
    // Note that RFC27 defines 31 as the max priority. Because our queue policy
    // layer sorts the pending jobs in lexicographical order
    // (<priority, t_submit, ...> and lower the better, we adjust the priority.
    job->priority = 31 - job->priority;
    if (ctx->queues.find (queue_name) == ctx->queues.end ()) {
        if (schedutil_alloc_respond_deny (ctx->schedutil, msg, NULL) < 0)
            flux_log_error (h, "%s: schedutil_alloc_respond_deny",
                            __FUNCTION__);
        errno = ENOENT;
        flux_log (h, LOG_DEBUG, "%s: queue (%s) doesn't exist",
                  __FUNCTION__, queue_name.c_str ());
        return;
    }

    job->jobspec = jobspec;
    job->msg = flux_msg_copy (msg, true);
    auto &queue = ctx->queues.at (queue_name);
    if (queue->insert (job) < 0) {
        flux_log_error (h, "%s: queue insert (id=%jd)", __FUNCTION__,
                       static_cast<intmax_t> (job->id));
        if (schedutil_alloc_respond_deny (ctx->schedutil, msg, NULL) < 0)
            flux_log_error (h, "%s: schedutil_alloc_respond_deny",
                            __FUNCTION__);
        return;
    }
    if (queue->run_sched_loop (static_cast<void *> (h), true) < 0
        || post_sched_loop (h, ctx->schedutil, ctx->queues) < 0) {
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
    std::shared_ptr<queue_policy_base_t> queue;
    std::string queue_name;

    if (schedutil_free_request_decode (msg, &id) < 0) {
        flux_log_error (h, "%s: schedutil_free_request_decode", __FUNCTION__);
        return;
    }
    if (ctx->find_queue (id, queue_name, queue) < 0) {
        flux_log_error (h, "%s: can't find queue for job (id=%jd)",
                        __FUNCTION__, static_cast<intmax_t> (id));
        return;
    }
    if ((queue->remove (id)) < 0) {
        flux_log_error (h, "%s: remove (queue=%s id=%jd)", __FUNCTION__,
                       queue_name.c_str (), static_cast<intmax_t> (id));
        return;
    }
    if (queue->run_sched_loop (static_cast<void *> (h), true) < 0) {
        flux_log_error (h, "%s: run_sched_loop", __FUNCTION__);
        return;
    }
    if (schedutil_free_respond (ctx->schedutil, msg) < 0) {
        flux_log_error (h, "%s: schedutil_free_respond", __FUNCTION__);
        return;
    }
    flux_log (h, LOG_DEBUG, "free succeeded (queue=%s id=%jd)",
             queue_name.c_str (), static_cast<intmax_t> (id));
    if (post_sched_loop (h, ctx->schedutil, ctx->queues) < 0) {
        flux_log_error (h, "%s: post_sched_loop", __FUNCTION__);
        return;
    }
}

void qmanager_cb_t::jobmanager_cancel_cb (flux_t *h, flux_jobid_t id,
                                          void *arg)
{
    std::shared_ptr<job_t> job;
    qmanager_cb_ctx_t *ctx = nullptr;
    ctx = static_cast<qmanager_cb_ctx_t *> (arg);
    std::shared_ptr<queue_policy_base_t> queue;
    std::string queue_name;

    if (ctx->find_queue (id, queue_name, queue) < 0) {
        flux_log_error (h, "%s: queue not found for job (id=%jd)",
                        __FUNCTION__, static_cast<intmax_t> (id));
        return;
    }
    if ((job = queue->lookup (id)) == nullptr
        || !job->is_pending ())
        return;
    if (queue->remove (id) < 0) {
        flux_log_error (h, "%s: remove job (%jd)", __FUNCTION__,
                       static_cast<intmax_t> (id));
        return;
    }
    if (schedutil_alloc_respond_cancel (ctx->schedutil,
                                        job->msg) < 0) {
        flux_log_error (h, "%s: schedutil_alloc_respond_cancel", __FUNCTION__);
        return;
    }
    flux_log (h, LOG_DEBUG, "%s (id=%jd)", "alloc cancelled",
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

void qmanager_safe_cb_t::jobmanager_cancel_cb (flux_t *h, flux_jobid_t id,
                                               void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (qmanager_cb_t::jobmanager_cancel_cb,
                            h, id, arg);
    if (exception_safe_wrapper.bad ())
        flux_log_error (h, "%s: %s", __FUNCTION__,
                        exception_safe_wrapper.get_err_message ());
}

int qmanager_safe_cb_t::post_sched_loop (flux_t *h, schedutil_t *schedutil,
                                         std::map<std::string,
                                                  std::shared_ptr<
                                                      queue_policy_base_t>> &queues)
{
    int rc;
    eh_wrapper_t exception_safe_wrapper;
    rc = exception_safe_wrapper (qmanager_cb_t::post_sched_loop,
                                 h, schedutil, queues);
    if (exception_safe_wrapper.bad ())
        flux_log_error (h, "%s: %s", __FUNCTION__,
                        exception_safe_wrapper.get_err_message ());
    return rc;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
