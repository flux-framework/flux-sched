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
}

#include "qmanager/modules/qmanager_callbacks.hpp"
#include "resource/libjobspec/jobspec.hpp"
#include "src/common/c++wrappers/eh_wrapper.hpp"

#include <jansson.hpp>

using namespace Flux;
using namespace Flux::Jobspec;
using namespace Flux::queue_manager;
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

int qmanager_cb_t::post_sched_loop (
    flux_t *h,
    schedutil_t *schedutil,
    std::map<std::string, std::shared_ptr<queue_policy_base_t>> &queues)
{
    int rc = -1;
    unsigned int qd = 0;
    std::shared_ptr<job_t> job = nullptr;
    for (auto &kv : queues) {
        const std::string &queue_name = kv.first;
        std::shared_ptr<queue_policy_base_t> &queue = kv.second;
        while ((job = queue->alloced_pop ()) != nullptr) {
            if (schedutil_alloc_respond_success_pack (schedutil,
                                                      job->msg,
                                                      job->schedule.R.c_str (),
                                                      "{ s:{s:n} }",
                                                      "sched",
                                                      "t_estimate")
                < 0) {
                flux_log_error (h,
                                "%s: schedutil_alloc_respond_pack (queue=%s)",
                                __FUNCTION__,
                                queue_name.c_str ());
                goto out;
            }
        }
        while ((job = queue->rejected_pop ()) != nullptr) {
            std::string note = "alloc denied due to type=\"" + job->note + "\"";
            if (schedutil_alloc_respond_deny (schedutil, job->msg, note.c_str ()) < 0) {
                flux_log_error (h,
                                "%s: schedutil_alloc_respond_deny (queue=%s)",
                                __FUNCTION__,
                                queue_name.c_str ());
                goto out;
            }
        }
        while ((job = queue->canceled_pop ()) != nullptr) {
            if (schedutil_alloc_respond_cancel (schedutil, job->msg) < 0) {
                flux_log_error (h, "%s: schedutil_alloc_respond_cancel", __FUNCTION__);
                goto out;
            }
        }
        for (job = queue->pending_begin (), qd = 0;
             job != nullptr && qd < queue->get_queue_depth ();
             job = queue->pending_next (), qd++) {
            // if old_at == at, then no reason to send this annotation again.
            if (job->schedule.at == job->schedule.old_at)
                continue;
            job->schedule.old_at = job->schedule.at;
            if (schedutil_alloc_respond_annotate_pack (schedutil,
                                                       job->msg,
                                                       "{ s:{s:f} }",
                                                       "sched",
                                                       "t_estimate",
                                                       static_cast<double> (job->schedule.at))) {
                flux_log_error (h, "%s: schedutil_alloc_respond_annotate_pack", __FUNCTION__);
                goto out;
            }
        }
        queue->reset_scheduled ();
    }
    rc = 0;

out:
    return rc;
}

/* The RFC 27 hello handshake occurs during scheduler initialization.  Its
 * purpose is to inform the scheduler of jobs that already have resources
 * allocated.  This callback is made once per job.  The callback should return
 * 0 on success or -1 on failure.  On failure, the job manager raises a fatal
 * exception on the job.
 *
 * Jobs that already have resources at hello need to be assigned to the correct
 * qmanager queue, but the queue is not provided in the hello metadata.
 * Therefore, jobspec is fetched from the KVS so that attributes.system.queue
 * can be extracted from it.
 *
 * Note that fluxion instantiates the "default" queue when no named queues
 * are configured.  Therefore, when the queue attribute is not defined, we
 * put the job in the default queue.
 *
 * Fail the job if its queue attribute (or lack thereof) no longer matches a
 * valid queue.  This can occur if queues have been reconfigured since job
 * submission.
 */
int qmanager_cb_t::jobmanager_hello_cb (flux_t *h, const flux_msg_t *msg, const char *R, void *arg)

{
    int rc = -1;
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
    const char *free_ranks = NULL;
    json_t *jobspec = NULL;
    flux_future_t *f = NULL;
    json_t *R_jsontmp = NULL;
    json_t *free_ranks_j = NULL;
    json_t *sched = NULL;
    json_error_t json_err;
    const char *R_final = NULL;

    /* Don't expect jobspec to be set here as it is not currently defined
     * in RFC 27.  However, add it anyway in case the hello protocol
     * evolves to include it.  If it is not set, it must be looked up.
     */
    if (flux_msg_unpack (msg,
                         "{s:I s:i s:i s:f s?s s?o}",
                         "id",
                         &id,
                         "priority",
                         &prio,
                         "userid",
                         &uid,
                         "t_submit",
                         &ts,
                         "free",
                         &free_ranks,
                         "jobspec",
                         &jobspec)
        < 0) {
        flux_log_error (h, "%s: flux_msg_unpack", __FUNCTION__);
        goto out;
    }
    if (!jobspec) {
        char key[64] = {0};
        if (flux_job_kvs_key (key, sizeof (key), id, "jobspec") < 0
            || !(f = flux_kvs_lookup (h, NULL, 0, key))
            || flux_kvs_lookup_get_unpack (f, "o", &jobspec) < 0) {
            flux_log_error (h, "%s", key);
            goto out;
        }
    }
    if (json_unpack (jobspec, "{s?{s?{s?s}}}", "attributes", "system", "queue", &qn_attr) < 0) {
        flux_log_error (h, "error parsing jobspec");
        goto out;
    }
    if (qn_attr)
        queue_name = qn_attr;
    else
        queue_name = ctx->opts.get_opt ().get_default_queue_name ();
    if (ctx->queues.find (queue_name) == ctx->queues.end ()) {
        flux_log (h,
                  LOG_ERR,
                  "%s: unknown queue name (id=%jd queue=%s)",
                  __FUNCTION__,
                  static_cast<intmax_t> (id),
                  queue_name.c_str ());
        goto out;
    }
    // if free ranks is populated, insert the free ranks into the scheduling key
    if (free_ranks) {
        if (!(R_jsontmp = json_loads (R, 0, &json_err))) {
            errno = ENOMEM;
            flux_log (h, LOG_ERR, "%s: json_loads", __FUNCTION__);
            goto out;
        }
        if ((sched = json_object_get (R_jsontmp, "scheduling")) == NULL) {
            R_final = R;
        } else {
            free_ranks_j = json_string (free_ranks);
            json_object_set (sched, "free_ranks", free_ranks_j);
            if (!(R_final = json_dumps (R_jsontmp, JSON_COMPACT))) {
                errno = ENOMEM;
                flux_log (h, LOG_ERR, "%s: json_dumps", __FUNCTION__);
                goto out;
            }
        }
    } else {
        R_final = R;
    }
    queue = ctx->queues.at (queue_name);
    running_job = std::make_shared<job_t> (job_state_kind_t::RUNNING,
                                           id,
                                           uid,
                                           calc_priority (prio),
                                           ts,
                                           R_final);

    if (queue->reconstruct (static_cast<void *> (h), running_job, R_out) < 0) {
        flux_log_error (h,
                        "%s: reconstruct (id=%jd queue=%s)",
                        __FUNCTION__,
                        static_cast<intmax_t> (id),
                        queue_name.c_str ());
        goto out;
    }
    flux_log (h,
              LOG_DEBUG,
              "requeue success (queue=%s id=%jd)",
              queue_name.c_str (),
              static_cast<intmax_t> (id));
    rc = 0;
out:
    flux_future_destroy (f);
    return rc;
}

void qmanager_cb_t::jobmanager_stats_get_cb (flux_t *h,
                                             flux_msg_handler_t *w,
                                             const flux_msg_t *msg,
                                             void *arg)
{
    qmanager_cb_ctx_t *ctx = static_cast<qmanager_cb_ctx_t *> (arg);

    json::value stats;
    stats.emplace_object ();
    json::value queues;
    queues.emplace_object ();
    json_object_set (stats.get (), "queues", queues.get ());
    for (auto &[qname, queue] : ctx->queues) {
        json::value qv;
        queue->to_json_value (qv);
        if (json_object_set (queues.get (), qname.c_str (), qv.get ()) < 0)
            throw std::system_error (errno, std::generic_category ());
    }
    char *resp = json_dumps (stats.get (), 0);
    if (flux_respond (h, msg, resp) < 0) {
        flux_log_error (h, "%s: flux_respond", __PRETTY_FUNCTION__);
    }
    free (resp);
}
void qmanager_cb_t::jobmanager_stats_clear_cb (flux_t *h,
                                               flux_msg_handler_t *w,
                                               const flux_msg_t *msg,
                                               void *arg)
{
    qmanager_cb_ctx_t *ctx = static_cast<qmanager_cb_ctx_t *> (arg);
}

void qmanager_cb_t::jobmanager_alloc_cb (flux_t *h, const flux_msg_t *msg, void *arg)
{
    qmanager_cb_ctx_t *ctx = nullptr;
    ctx = static_cast<qmanager_cb_ctx_t *> (arg);
    Flux::Jobspec::Jobspec jobspec_obj;
    std::string queue_name = ctx->opts.get_opt ().get_default_queue_name ();
    std::shared_ptr<job_t> job = std::make_shared<job_t> ();
    flux_jobid_t id;
    unsigned int priority;
    uint32_t userid;
    double t_submit;
    json_t *jobspec;
    char *jobspec_str = NULL;
    char errbuf[80];

    if (flux_msg_unpack (msg,
                         "{s:I s:i s:i s:f s:o}",
                         "id",
                         &id,
                         "priority",
                         &priority,
                         "userid",
                         &userid,
                         "t_submit",
                         &t_submit,
                         "jobspec",
                         &jobspec)
        < 0) {
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
    try {
        jobspec_obj = Flux::Jobspec::Jobspec (jobspec_str);
    } catch (const Flux::Jobspec::parse_error &e) {
        if (schedutil_alloc_respond_deny (ctx->schedutil, msg, e.what ()) < 0)
            flux_log_error (h, "%s: schedutil_alloc_respond_deny", __FUNCTION__);
        free (jobspec_str);
        return;
    }
    if (jobspec_obj.attributes.system.queue != "")
        queue_name = jobspec_obj.attributes.system.queue;
    job->jobspec = jobspec_str;
    free (jobspec_str);
    if (ctx->queues.find (queue_name) == ctx->queues.end ()) {
        snprintf (errbuf, sizeof (errbuf), "queue (%s) doesn't exist", queue_name.c_str ());
        if (schedutil_alloc_respond_deny (ctx->schedutil, msg, errbuf) < 0)
            flux_log_error (h, "%s: schedutil_alloc_respond_deny", __FUNCTION__);
        errno = ENOENT;
        return;
    }

    job->msg = flux_msg_copy (msg, true);
    auto &queue = ctx->queues.at (queue_name);
    if (queue->insert (job) < 0) {
        snprintf (errbuf,
                  sizeof (errbuf),
                  "fluxion could not insert job into queue %s",
                  queue_name.c_str ());
        flux_log_error (h,
                        "%s: queue insert (id=%jd)",
                        __FUNCTION__,
                        static_cast<intmax_t> (job->id));
        if (schedutil_alloc_respond_deny (ctx->schedutil, msg, errbuf) < 0)
            flux_log_error (h, "%s: schedutil_alloc_respond_deny", __FUNCTION__);
        return;
    }
}

void qmanager_cb_t::jobmanager_free_cb (flux_t *h, const flux_msg_t *msg, const char *R, void *arg)
{
    flux_jobid_t id;
    json_t *Res;
    int final = 0;
    const char *Rstr = NULL;
    qmanager_cb_ctx_t *ctx = nullptr;
    ctx = static_cast<qmanager_cb_ctx_t *> (arg);
    std::shared_ptr<queue_policy_base_t> queue;
    std::string queue_name;

    if (flux_request_unpack (msg, NULL, "{s:I s:O s?b}", "id", &id, "R", &Res, "final", &final)
        < 0) {
        flux_log_error (h, "%s: flux_request_unpack", __FUNCTION__);
        return;
    }
    if (!(Rstr = json_dumps (Res, JSON_COMPACT))) {
        errno = ENOMEM;
        flux_log (h, LOG_ERR, "%s: json_dumps ", __FUNCTION__);
        goto done;
    }
    if (ctx->find_queue (id, queue_name, queue) < 0) {
        flux_log_error (h,
                        "%s: can't find queue for job (id=%jd)",
                        __FUNCTION__,
                        static_cast<intmax_t> (id));
        goto done;
    }
    if ((queue->remove (static_cast<void *> (h), id, final, Rstr)) < 0) {
        flux_log_error (h,
                        "%s: remove (queue=%s id=%jd)",
                        __FUNCTION__,
                        queue_name.c_str (),
                        static_cast<intmax_t> (id));
        goto done;
    }

done:
    json_decref (Res);
    free ((void *)Rstr);
    return;
}

void qmanager_cb_t::jobmanager_cancel_cb (flux_t *h, const flux_msg_t *msg, void *arg)
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
        flux_log_error (h,
                        "%s: queue not found for job (id=%jd)",
                        __FUNCTION__,
                        static_cast<intmax_t> (id));
        return;
    }
    if ((job = queue->lookup (id)) == nullptr || !job->is_pending ())
        return;
    if (queue->remove_pending (job.get ()) < 0) {
        flux_log_error (h, "%s: remove job (%jd)", __FUNCTION__, static_cast<intmax_t> (id));
        return;
    }
}

void qmanager_cb_t::jobmanager_prioritize_cb (flux_t *h, const flux_msg_t *msg, void *arg)
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
            flux_log_error (h, "%s: invalid prioritize entry", __FUNCTION__);
            return;
        }

        if (ctx->find_queue (id, queue_name, queue) < 0) {
            flux_log_error (h,
                            "%s: queue not found for job (id=%jd)",
                            __FUNCTION__,
                            static_cast<intmax_t> (id));
            continue;
        }

        if (queue->pending_reprioritize (id, calc_priority (priority)) < 0) {
            if (errno == ENOENT) {
                flux_log_error (h,
                                "invalid job reprioritized (id=%jd)",
                                static_cast<intmax_t> (id));
                continue;
            } else if (errno == EINVAL) {
                flux_log_error (h,
                                "reprioritized non-pending job (id=%jd)",
                                static_cast<intmax_t> (id));
                continue;
            }
            flux_log_error (h,
                            "%s: queue pending_reprioritize (id=%jd)",
                            __FUNCTION__,
                            static_cast<intmax_t> (id));
            return;
        }
    }
}

void qmanager_cb_t::prep_watcher_cb (flux_reactor_t *r, flux_watcher_t *w, int revents, void *arg)
{
    qmanager_cb_ctx_t *ctx = nullptr;
    ctx = static_cast<qmanager_cb_ctx_t *> (arg);
    ctx->pls_sched_loop = false;
    ctx->pls_post_loop = false;
    for (auto &kv : ctx->queues) {
        std::shared_ptr<queue_policy_base_t> &queue = kv.second;
        ctx->pls_sched_loop = ctx->pls_sched_loop || queue->is_schedulable ();
        ctx->pls_post_loop = ctx->pls_post_loop || queue->is_scheduled ();
    }
    if (ctx->pls_sched_loop || ctx->pls_post_loop)
        flux_watcher_start (ctx->idle);
}

void qmanager_cb_t::check_watcher_cb (flux_reactor_t *r, flux_watcher_t *w, int revents, void *arg)
{
    qmanager_cb_ctx_t *ctx = nullptr;
    ctx = static_cast<qmanager_cb_ctx_t *> (arg);

    if (ctx->idle)
        flux_watcher_stop (ctx->idle);
    if (!ctx->pls_sched_loop && !ctx->pls_post_loop)
        return;
    if (ctx->pls_sched_loop) {
        for (auto &kv : ctx->queues) {
            std::shared_ptr<queue_policy_base_t> &queue = kv.second;
            if (queue->run_sched_loop (static_cast<void *> (ctx->h), true) < 0) {
                if (errno == EAGAIN)
                    continue;
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

int qmanager_safe_cb_t::jobmanager_hello_cb (flux_t *h,
                                             const flux_msg_t *msg,
                                             const char *R,
                                             void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    int rc = exception_safe_wrapper (qmanager_cb_t::jobmanager_hello_cb, h, msg, R, arg);
    if (exception_safe_wrapper.bad ())
        flux_log_error (h, "%s: %s", __FUNCTION__, exception_safe_wrapper.get_err_message ());
    return rc;
}

void qmanager_safe_cb_t::jobmanager_stats_get_cb (flux_t *h,
                                                  flux_msg_handler_t *w,
                                                  const flux_msg_t *msg,
                                                  void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (qmanager_cb_t::jobmanager_stats_get_cb, h, w, msg, arg);
    if (exception_safe_wrapper.bad ())
        flux_log_error (h, "%s: %s", __FUNCTION__, exception_safe_wrapper.get_err_message ());
}

void qmanager_safe_cb_t::jobmanager_stats_clear_cb (flux_t *h,
                                                    flux_msg_handler_t *w,
                                                    const flux_msg_t *msg,
                                                    void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (qmanager_cb_t::jobmanager_stats_clear_cb, h, w, msg, arg);
    if (exception_safe_wrapper.bad ())
        flux_log_error (h, "%s: %s", __FUNCTION__, exception_safe_wrapper.get_err_message ());
}

void qmanager_safe_cb_t::jobmanager_alloc_cb (flux_t *h, const flux_msg_t *msg, void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (qmanager_cb_t::jobmanager_alloc_cb, h, msg, arg);
    if (exception_safe_wrapper.bad ())
        flux_log_error (h, "%s: %s", __FUNCTION__, exception_safe_wrapper.get_err_message ());
}

void qmanager_safe_cb_t::jobmanager_free_cb (flux_t *h,
                                             const flux_msg_t *msg,
                                             const char *R,
                                             void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (qmanager_cb_t::jobmanager_free_cb, h, msg, R, arg);
    if (exception_safe_wrapper.bad ())
        flux_log_error (h, "%s: %s", __FUNCTION__, exception_safe_wrapper.get_err_message ());
}

void qmanager_safe_cb_t::jobmanager_cancel_cb (flux_t *h, const flux_msg_t *msg, void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (qmanager_cb_t::jobmanager_cancel_cb, h, msg, arg);
    if (exception_safe_wrapper.bad ())
        flux_log_error (h, "%s: %s", __FUNCTION__, exception_safe_wrapper.get_err_message ());
}

void qmanager_safe_cb_t::jobmanager_prioritize_cb (flux_t *h, const flux_msg_t *msg, void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (qmanager_cb_t::jobmanager_prioritize_cb, h, msg, arg);
    if (exception_safe_wrapper.bad ())
        flux_log_error (h, "%s: %s", __FUNCTION__, exception_safe_wrapper.get_err_message ());
}

void qmanager_safe_cb_t::prep_watcher_cb (flux_reactor_t *r,
                                          flux_watcher_t *w,
                                          int revents,
                                          void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (qmanager_cb_t::prep_watcher_cb, r, w, revents, arg);
    if (exception_safe_wrapper.bad ()) {
        flux_t *h = flux_handle_watcher_get_flux (w);
        flux_log_error (h, "%s: %s", __FUNCTION__, exception_safe_wrapper.get_err_message ());
    }
}

void qmanager_safe_cb_t::check_watcher_cb (flux_reactor_t *r,
                                           flux_watcher_t *w,
                                           int revents,
                                           void *arg)
{
    eh_wrapper_t exception_safe_wrapper;
    exception_safe_wrapper (qmanager_cb_t::check_watcher_cb, r, w, revents, arg);
    if (exception_safe_wrapper.bad ()) {
        flux_t *h = flux_handle_watcher_get_flux (w);
        flux_log_error (h, "%s: %s", __FUNCTION__, exception_safe_wrapper.get_err_message ());
    }
}

int qmanager_safe_cb_t::post_sched_loop (
    flux_t *h,
    schedutil_t *schedutil,
    std::map<std::string, std::shared_ptr<queue_policy_base_t>> &queues)
{
    int rc;
    eh_wrapper_t exception_safe_wrapper;
    rc = exception_safe_wrapper (qmanager_cb_t::post_sched_loop, h, schedutil, queues);
    if (exception_safe_wrapper.bad ())
        flux_log_error (h, "%s: %s", __FUNCTION__, exception_safe_wrapper.get_err_message ());
    return rc;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
