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

static unsigned int calc_priority (unsigned int priority)
{
    // RFC27 defines 4294967295 as the max priority. Because
    // our queue policy layer sorts the pending jobs in
    // lexicographical order (<priority, t_submit, ...>) and lower the
    // better, we need to calculate an adjusted priority.
    return (4294967295 - priority);
}

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
    unsigned int qd = 0;
    std::shared_ptr<job_t> job = nullptr;
    for (auto& kv: queues) {
        const std::string &queue_name = kv.first;
        std::shared_ptr<queue_policy_base_t> &queue = kv.second;
        while ( (job = queue->alloced_pop ()) != nullptr) {
            if (schedutil_alloc_respond_success_pack (schedutil, job->msg,
                                                      job->schedule.R.c_str (),
                                                      "{ s:{s:s s:n} }",
                                                      "sched",
                                                          "queue", queue_name.c_str (),
                                                          "t_estimate") < 0) {
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
        while ( (job = queue->canceled_pop ()) != nullptr) {
            if (schedutil_alloc_respond_cancel (schedutil, job->msg) < 0) {
                flux_log_error (h, "%s: schedutil_alloc_respond_cancel",
                                __FUNCTION__);
                goto out;
            }
            flux_log (h, LOG_DEBUG, "%s (id=%jd queue=%s)", "alloc canceled",
                      static_cast<intmax_t> (job->id), queue_name.c_str ());
        }
        for (job = queue->pending_begin (), qd = 0;
             job != nullptr && qd < queue->get_queue_depth ();
             job = queue->pending_next (), qd++) {
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
        queue->reset_scheduled ();
    }
    rc = 0;

out:
    return rc;
}

int qmanager_cb_t::jobmanager_hello_cb (flux_t *h, const flux_msg_t *msg,
                                        const char *R, void *arg)

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
    flux_jobid_t id;
    unsigned int prio;
    uint32_t uid;
    double ts;

    if (flux_msg_unpack (msg,
                         "{s:I s:i s:i s:f}",
                         "id", &id,
                         "priority", &prio,
                         "userid", &uid,
                         "t_submit", &ts) < 0) {
        flux_log_error (h, "%s: flux_msg_unpack", __FUNCTION__);
        goto out;
    }

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
    running_job = std::make_shared<job_t> (job_state_kind_t::RUNNING,
                                                   id, uid, calc_priority (prio),
                                                   ts, R);

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
                                         void *arg)
{
    qmanager_cb_ctx_t *ctx = nullptr;
    ctx = static_cast<qmanager_cb_ctx_t *> (arg);
    Flux::Jobspec::Jobspec jobspec_obj;
    std::string queue_name = ctx->opts.get_opt ().get_default_queue ();
    std::shared_ptr<job_t> job = std::make_shared<job_t> ();
    flux_jobid_t id;
    unsigned int priority;
    uint32_t userid;
    double t_submit;
    json_t *jobspec;
    char *jobspec_str = NULL;

    if (flux_msg_unpack (msg,
                         "{s:I s:i s:i s:f s:o}",
                         "id", &id,
                         "priority", &priority,
                         "userid", &userid,
                         "t_submit", &t_submit,
                         "jobspec", &jobspec) < 0) {
        flux_log_error (h, "%s: flux_msg_unpack", __FUNCTION__);
        return;
    }
    if (!(jobspec_str = json_dumps (jobspec, JSON_COMPACT))) {
        errno = ENOMEM;
        flux_log (h, LOG_ERR, "%s: json_dumps", __FUNCTION__);
        return;
    }
    job->id = id;
    job->userid = userid;
    job->t_submit = t_submit;
    job->priority = calc_priority (priority);
    jobspec_obj = Flux::Jobspec::Jobspec (jobspec_str);
    if (jobspec_obj.attributes.system.queue != "")
        queue_name = jobspec_obj.attributes.system.queue;
    job->jobspec = jobspec_str;
    free (jobspec_str);
    if (ctx->queues.find (queue_name) == ctx->queues.end ()) {
        if (schedutil_alloc_respond_deny (ctx->schedutil, msg, NULL) < 0)
            flux_log_error (h, "%s: schedutil_alloc_respond_deny",
                            __FUNCTION__);
        errno = ENOENT;
        flux_log (h, LOG_DEBUG, "%s: queue (%s) doesn't exist",
                  __FUNCTION__, queue_name.c_str ());
        return;
    }

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
}

void qmanager_cb_t::jobmanager_free_cb (flux_t *h, const flux_msg_t *msg,
                                        const char *R, void *arg)
{
    flux_jobid_t id;
    qmanager_cb_ctx_t *ctx = nullptr;
    ctx = static_cast<qmanager_cb_ctx_t *> (arg);
    std::shared_ptr<queue_policy_base_t> queue;
    std::string queue_name;

    if (flux_request_unpack (msg, NULL, "{s:I}", "id", &id) < 0) {
        flux_log_error (h, "%s: flux_request_unpack", __FUNCTION__);
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
    if (schedutil_free_respond (ctx->schedutil, msg) < 0) {
        flux_log_error (h, "%s: schedutil_free_respond", __FUNCTION__);
        return;
    }
    flux_log (h, LOG_DEBUG, "free succeeded (queue=%s id=%jd)",
             queue_name.c_str (), static_cast<intmax_t> (id));
}

void qmanager_cb_t::jobmanager_cancel_cb (flux_t *h, const flux_msg_t *msg,
                                          void *arg)
{
    std::shared_ptr<job_t> job;
    qmanager_cb_ctx_t *ctx = nullptr;
    ctx = static_cast<qmanager_cb_ctx_t *> (arg);
    std::shared_ptr<queue_policy_base_t> queue;
    std::string queue_name;
    flux_jobid_t id;

    if (flux_msg_unpack (msg, "{s:I}", "id", &id) < 0) {
        flux_log_error (h, "%s: flux_msg_unpack", __FUNCTION__);
        return;
    }
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
}

void qmanager_cb_t::jobmanager_prioritize_cb (flux_t *h, const flux_msg_t *msg,
                                              void *arg)
{
    qmanager_cb_ctx_t *ctx = nullptr;
    ctx = static_cast<qmanager_cb_ctx_t *> (arg);
    std::shared_ptr<queue_policy_base_t> queue;
    std::string queue_name;
    json_t *jobs;
    size_t index;
    json_t *arr;

    if (flux_msg_unpack (msg, "{s:o}", "jobs", &jobs) < 0) {
        flux_log_error (h, "%s: flux_msg_unpack", __FUNCTION__);
        return;
    }

    json_array_foreach (jobs, index, arr) {
        flux_jobid_t id;
        unsigned int priority;

        if (json_unpack (arr, "[I,i]", &id, &priority) < 0) {
            flux_log_error (h, "%s: invalid prioritize entry",
                            __FUNCTION__);
            return;
        }

        if (ctx->find_queue (id, queue_name, queue) < 0) {
            flux_log_error (h, "%s: queue not found for job (id=%jd)",
                            __FUNCTION__, static_cast<intmax_t> (id));
            continue;
        }

        if (queue->pending_reprioritize (id, calc_priority (priority)) < 0) {
            if (errno == ENOENT) {
                flux_log_error (h, "invalid job reprioritized (id=%jd)",
                                static_cast<intmax_t> (id));
                continue;
            }
            else if (errno == EINVAL) {
                flux_log_error (h, "reprioritized non-pending job (id=%jd)",
                                static_cast<intmax_t> (id));
                continue;
            }
            flux_log_error (h, "%s: queue pending_reprioritize (id=%jd)",
                            __FUNCTION__, static_cast<intmax_t> (id));
            return;
        }
    }
}

void qmanager_cb_t::prep_watcher_cb (flux_reactor_t *r, flux_watcher_t *w,
                                     int revents, void *arg)
{
    qmanager_cb_ctx_t *ctx = nullptr;
    ctx = static_cast<qmanager_cb_ctx_t *> (arg);
    ctx->pls_sched_loop = false;
    ctx->pls_post_loop = false;
    for (auto &kv: ctx->queues) {
        std::shared_ptr<queue_policy_base_t> &queue = kv.second;
        ctx->pls_sched_loop = ctx->pls_sched_loop
                              || (queue->is_schedulable ()
                                  && !queue->is_sched_loop_active ());
        ctx->pls_post_loop = ctx->pls_post_loop
                              || queue->is_scheduled ();
    }
    if (ctx->pls_sched_loop || ctx->pls_post_loop)
        flux_watcher_start (ctx->idle);
}

void qmanager_cb_t::check_watcher_cb (flux_reactor_t *r, flux_watcher_t *w,
                                     int revents, void *arg)
{
    qmanager_cb_ctx_t *ctx = nullptr;
    ctx = static_cast<qmanager_cb_ctx_t *> (arg);

    if (ctx->idle)
        flux_watcher_stop (ctx->idle);
    if (!ctx->pls_sched_loop && !ctx->pls_post_loop)
        return;
    if (ctx->pls_sched_loop) {
        for (auto &kv: ctx->queues) {
            std::shared_ptr<queue_policy_base_t> &queue = kv.second;
            if (queue->run_sched_loop (static_cast<void *> (ctx->h), true) < 0) {
                flux_log_error (ctx->h, "%s: run_sched_loop", __FUNCTION__);
                return;
            }
         }
    }
    if (post_sched_loop (ctx->h, ctx->schedutil, ctx->queues) < 0) {
        flux_log_error (ctx->h, "%s: post_sched_loop", __FUNCTION__);
        return;
    }
}

int qmanager_safe_cb_t::jobmanager_hello_cb (flux_t *h, const flux_msg_t *msg,
                                             const char *R, void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    int rc = exception_safe_wrapper (qmanager_cb_t::jobmanager_hello_cb,
                                     h, msg, R, arg);
    if (exception_safe_wrapper.bad ())
        flux_log_error (h, "%s: %s", __FUNCTION__,
                        exception_safe_wrapper.get_err_message ());
    return rc;
}

void qmanager_safe_cb_t::jobmanager_alloc_cb (flux_t *h, const flux_msg_t *msg,
                                              void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (qmanager_cb_t::jobmanager_alloc_cb,
                            h, msg, arg);
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

void qmanager_safe_cb_t::jobmanager_cancel_cb (flux_t *h, const flux_msg_t *msg,
                                               void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (qmanager_cb_t::jobmanager_cancel_cb,
                            h, msg, arg);
    if (exception_safe_wrapper.bad ())
        flux_log_error (h, "%s: %s", __FUNCTION__,
                        exception_safe_wrapper.get_err_message ());
}

void qmanager_safe_cb_t::jobmanager_prioritize_cb (flux_t *h,
                                                   const flux_msg_t *msg,
                                                   void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (qmanager_cb_t::jobmanager_prioritize_cb,
                            h, msg, arg);
    if (exception_safe_wrapper.bad ())
        flux_log_error (h, "%s: %s", __FUNCTION__,
                        exception_safe_wrapper.get_err_message ());
}

void qmanager_safe_cb_t::prep_watcher_cb (flux_reactor_t *r, flux_watcher_t *w,
                                          int revents, void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (qmanager_cb_t::prep_watcher_cb, r, w, revents, arg);
    if (exception_safe_wrapper.bad ()) {
        flux_t *h = flux_handle_watcher_get_flux (w);
        flux_log_error (h, "%s: %s", __FUNCTION__,
                        exception_safe_wrapper.get_err_message ());
    }
}

void qmanager_safe_cb_t::check_watcher_cb (flux_reactor_t *r, flux_watcher_t *w,
                                           int revents, void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (qmanager_cb_t::check_watcher_cb,
                            r, w, revents, arg);
    if (exception_safe_wrapper.bad ()) {
        flux_t *h = flux_handle_watcher_get_flux (w);
        flux_log_error (h, "%s: %s", __FUNCTION__,
                        exception_safe_wrapper.get_err_message ());
    }
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
