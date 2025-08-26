/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef QUEUE_POLICY_BASE_HPP
#define QUEUE_POLICY_BASE_HPP

#include <flux/core/job.h>
extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>
}

#include <cassert>
#include <map>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <memory>
#include <cstdint>
#include <tuple>
#include <jansson.hpp>
#include <iostream>

#include "resource/reapi/bindings/c++/reapi.hpp"
#include "qmanager/config/queue_system_defaults.hpp"

namespace Flux {
namespace queue_manager {

enum class job_state_kind_t { INIT, PENDING, REJECTED, RUNNING, ALLOC_RUNNING, CANCELED, COMPLETE };

/*! Type to store schedule information such as the
 *  allocated or reserved (for backfill) resource set (R).
 */
struct schedule_t {
    schedule_t () = default;
    schedule_t (const std::string &r) : R (r)
    {
    }
    schedule_t (schedule_t &&s) = default;
    schedule_t (const schedule_t &s) = default;
    schedule_t &operator= (schedule_t &&s) = default;
    schedule_t &operator= (const schedule_t &s) = default;
    std::string R = "";
    bool reserved = false;
    int64_t at = 0;
    int64_t old_at = 0;
    double ov = 0.0f;
};

/*! Type to store various time stamps for queuing
 */
struct t_stamps_t {
    uint64_t pending_ts = 0;
    uint64_t running_ts = 0;
    uint64_t rejected_ts = 0;
    uint64_t complete_ts = 0;
    uint64_t canceled_ts = 0;
};

/*! Helper typedef for use as a key in job state maps
 */

using pending_key = std::tuple<unsigned int, double, uint64_t>;

using job_map_t = std::map<pending_key, flux_jobid_t>;
using job_map_iter = job_map_t::iterator;

/*! Type to store a job's attributes.
 */
class job_t {
   public:
    ~job_t ()
    {
        flux_msg_destroy (msg);
    }
    job_t () = default;
    job_t (job_state_kind_t s,
           flux_jobid_t jid,
           uint32_t uid,
           unsigned int p,
           double t_s,
           const std::string &R)
        : state (s), id (jid), userid (uid), priority (p), t_submit (t_s), schedule (R)
    {
    }
    job_t (job_t &&j) = default;
    job_t (const job_t &j) = default;
    job_t &operator= (job_t &&s) = default;
    job_t &operator= (const job_t &s) = default;

    bool is_pending ()
    {
        return state == job_state_kind_t::PENDING;
    }
    pending_key get_key ()
    {
        return {priority, t_submit, t_stamps.pending_ts};
    }

    flux_msg_t *msg = NULL;
    job_state_kind_t state = job_state_kind_t::INIT;
    flux_jobid_t id = 0;
    uint32_t userid = 0;
    unsigned int priority = 0;
    double t_submit = 0.0f;
    std::string jobspec = "";
    std::string note = "";
    t_stamps_t t_stamps;
    schedule_t schedule;
};

/*! Queue policy base interface abstract class. Derived classes must
 *  implement its run_sched_loop and destructor methods. Insert, remove
 *  and pending_pop interface implementations are provided through
 *  its parent class (detail::queue_policy_base_impl_t).
 */
class queue_policy_base_t : public resource_model::queue_adapter_base_t {
   public:
    /*! The destructor that must be implemented by derived classes.
     *
     */
    virtual ~queue_policy_base_t () {};

    /*! The main schedule loop interface that must be implemented
     *  by derived classes: how a derived class implements this method
     *  must determine its queueing policy. For example, a pedantic FCFS
     *  class should only iterate through the first set of jobs in the
     *  pending-job queue which it can schedule using the underlying
     *  resource match infrastructure.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module such as qmanager, it is expected
     *                   to be a pointer to a flux_t object.
     *  \param use_alloced_queue
     *                   Boolean indicating if you want to use the
     *                   allocated job queue or not. This affects the
     *                   alloced_pop method.
     *  \return          0 on success; -1 on error; 1 when a previous
     *                   loop invocation is still active under asynchronous
     *                   execution.
     *                       EINVAL: invalid argument.
     */
    virtual int run_sched_loop (void *h, bool use_alloced_queue) = 0;

    /*! This method must implement any logic required to cancel an in-progress
     *  scheduling loop. Since many policies may complete the entire loop during
     *  the execution of the call, and if the loop is delegated to resource
     *  cancelation may be best effort, a default is provided that does nothing
     *  if the sched loop is inactive, and raises an error if it's active.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module such as qmanager, it is expected
     *                   to be a pointer to a flux_t object.
     *  \return          0 on success; -1 on error; 1 when a previous
     *                   loop invocation is still active under asynchronous
     *                   execution.
     *                       EINVAL: invalid argument.
     */
    virtual int cancel_sched_loop ()
    {
        if (is_sched_loop_active ()) {
            errno = EINVAL;
            return -1;
        }
        return 0;
    }

    /*! Resource reconstruct interface that must be implemented by
     *  derived classes.
     *
     * \param h          Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module such as qmanager, it is expected
     *                   to be a pointer to a flux_t object.
     * \param job        shared pointer to a running job whose resource
     *                   state is being requested to be reconstructed.
     *                   job->schedule.R is the requested R.
     * \param ret_R      Replied R (must be equal to job->schedule.R
     *                   if succeeded).
     * \return           0 on success; -1 on error.
     *                       EINVAL: invalid argument.
     *                       ENOMEM: out of memory.
     *                       ERANGE: out of range request.
     *                       EPROTO: job->schedule.R doesn't comply.
     *                       ENOTSUP: job->schedule.R has unsupported feature.
     */
    virtual int reconstruct_resource (void *h, std::shared_ptr<job_t> job, std::string &ret_R) = 0;

    /// @brief move any jobs in blocked state to pending
    void process_provisional_reconsider ()
    {
        if (!m_pending_reconsider)
            return;
        m_pending_reconsider = false;
        auto cnt = m_blocked.size ();
        m_pending.merge (m_blocked);
        assert (m_blocked.size () == 0);
        if (cnt > 0) {
            set_schedulability (true);
        }
    }

    /// @brief move any jobs in blocked state to pending
    void reconsider_blocked_jobs ()
    {
        m_pending_reconsider = true;
        if (!is_sched_loop_active ()) {
            process_provisional_reconsider ();
        }
    }

    /*! Set queue parameters. Can be called multiple times.
     *
     * \param params     comma-delimited key-value pairs string
     *                   (e.g., "queue-depth=1024,foo=bar")
     * \return           0 on success; -1 on error.
     *                       EINVAL: invalid argument.
     */
    int set_queue_params (const std::string &params)
    {
        return set_params (params, m_qparams);
    }

    /*! Set queue policy parameters. Can be called multiple times.
     *
     * \param params     comma-delimited key-value pairs string
     *                   (e.g., "reservation-depth=10,foo=bar")
     * \return           0 on success; -1 on error.
     *                       EINVAL: invalid argument.
     */
    int set_policy_params (const std::string &params)
    {
        return set_params (params, m_pparams);
    }

    /*! Apply the set policy parameters to the queuing policy.
     */
    virtual int apply_params ()
    {
        int rc = 0;
        int depth = 0;
        try {
            std::unordered_map<std::string, std::string>::const_iterator i;
            if ((i = m_qparams.find ("max-queue-depth")) != m_qparams.end ()) {
                // We pre-check the input string to see if it is a positive number
                // before passing it to std::stoi. This works around issues
                // in some compilers where std::stoi aborts on certain
                // invalid input string with some debug flags (Issue #808).
                if (!is_number (i->second)) {
                    errno = EINVAL;
                    rc += -1;
                } else {
                    if ((depth = std::stoi (i->second)) < 1) {
                        errno = ERANGE;
                        rc += -1;
                    } else {
                        m_max_queue_depth = depth;
                        if (static_cast<unsigned> (depth) < m_queue_depth) {
                            m_queue_depth = depth;
                        }
                    }
                }
            }
            if ((i = m_qparams.find ("queue-depth")) != m_qparams.end ()) {
                // We pre-check the input string to see if it is a positive number
                // before passing it to std::stoi. This works around issues
                // in some compilers where std::stoi aborts on certain
                // invalid input string with some debug flags (Issue #808).
                if (!is_number (i->second)) {
                    errno = EINVAL;
                    rc += -1;
                } else {
                    if ((depth = std::stoi (i->second)) < 1) {
                        errno = ERANGE;
                        rc += -1;
                    } else {
                        if (static_cast<unsigned> (depth) < m_max_queue_depth) {
                            m_queue_depth = depth;
                        } else {
                            m_queue_depth = m_max_queue_depth;
                        }
                    }
                }
            }
        } catch (const std::invalid_argument &e) {
            rc = -1;
            errno = EINVAL;
        } catch (const std::out_of_range &e) {
            rc = -1;
            errno = ERANGE;
        }
        return rc;
    }

    /*! Get queue and policy parameters.
     *
     * \param q_p        string to which to print queue parameters
     *                   (e.g., "queue-depth=1024,foo=bar")
     * \param p_p        string to which to print queue parameters
     *                   (e.g., "reservation-depth=1024,foo=bar")
     */
    void get_params (std::string &q_p, std::string &p_p) const
    {
        std::unordered_map<std::string, std::string>::const_iterator i;
        for (i = m_qparams.begin (); i != m_qparams.end (); i++) {
            if (!q_p.empty ())
                q_p += std::string (",");
            q_p += i->first + std::string ("=") + i->second;
        }
        for (i = m_pparams.begin (); i != m_pparams.end (); i++) {
            if (!p_p.empty ())
                p_p += std::string (",");
            p_p += i->first + std::string ("=") + i->second;
        }
    }

    virtual const std::string_view policy () const = 0;

    /*! Write json stats into the json::value parameter
     *
     */
    virtual void to_json_value (json::value &jv) const
    {
        json::value qparams;
        to_json (qparams, m_qparams);
        json::value pparams;
        to_json (pparams, m_pparams);
        char buf[128] = {};
        auto add_queue = [&] (json_t *a, auto &map) {
            for (auto &[k, jobid] : map) {
                if (flux_job_id_encode (jobid, "f58plain", buf, sizeof buf) < 0)
                    json_array_append_new (a, json_integer (jobid));
                else
                    json_array_append_new (a, json_string (buf));
            }
        };
        json::value pending;
        pending.emplace_object ();
        json::value pending_arr;
        pending_arr.emplace_array ();
        json_object_set (pending.get (), "pending", pending_arr.get ());
        add_queue (pending_arr.get (), m_pending);
        pending_arr.emplace_array ();
        json_object_set (pending.get (), "pending_provisional", pending_arr.get ());
        add_queue (pending_arr.get (), m_pending_provisional);
        pending_arr.emplace_array ();
        json_object_set (pending.get (), "blocked", pending_arr.get ());
        add_queue (pending_arr.get (), m_blocked);

        json::value scheduled;
        scheduled.emplace_object ();
        json::value scheduled_arr;
        scheduled_arr.emplace_array ();
        json_object_set (scheduled.get (), "running", scheduled_arr.get ());
        add_queue (scheduled_arr.get (), m_running);
        scheduled_arr.emplace_array ();
        json_object_set (scheduled.get (), "rejected", scheduled_arr.get ());
        add_queue (scheduled_arr.get (), m_rejected);
        scheduled_arr.emplace_array ();
        json_object_set (scheduled.get (), "canceled", scheduled_arr.get ());
        add_queue (scheduled_arr.get (), m_canceled);

        json_error_t err = {0};
        jv = json::value (json::no_incref{},
                          json_pack_ex (&err,
                                        0,
                                        // begin object
                                        "{"
                                        // policy
                                        "s:s%"
                                        // queue_depth
                                        "s:I"
                                        // max_queue_depth
                                        "s:I"
                                        // queue parameters
                                        "s:O"
                                        // policy parameters
                                        "s:O"
                                        // action counts
                                        "s:o"
                                        // pending queues
                                        "s:O"
                                        // scheduled queues
                                        "s:O"
                                        // end object
                                        "}",
                                        // VALUE START
                                        // policy, str+length style
                                        "policy",
                                        this->policy ().data (),
                                        this->policy ().length (),
                                        // queue_depth
                                        "queue_depth",
                                        (json_int_t)m_queue_depth,
                                        // max_queue_depth
                                        "max_queue_depth",
                                        (json_int_t)m_max_queue_depth,
                                        // queue parameters
                                        "queue_parameters",
                                        qparams.get (),
                                        // policy parameters
                                        "policy_parameters",
                                        pparams.get (),
                                        // action counts
                                        "action_counts",
                                        json_pack ("{s:I s:I s:I s:I s:I s:I s:I}",
                                                   "pending",
                                                   m_pq_cnt,
                                                   "running",
                                                   m_rq_cnt,
                                                   "reserved",
                                                   m_oq_cnt,
                                                   "rejected",
                                                   m_dq_cnt,
                                                   "complete",
                                                   m_cq_cnt,
                                                   "cancelled",
                                                   m_cancel_cnt,
                                                   "reprioritized",
                                                   m_reprio_cnt),
                                        // pending queues
                                        "pending_queues",
                                        pending.get (),
                                        // scheduled queues
                                        "scheduled_queues",
                                        scheduled.get ()));
        if (!jv.get ()) {
            throw std::runtime_error (err.text);
        }
    }

    /*! Return the queue depth used for this queue. The queue depth
     *  is the depth of its pending-job queue only upto which it
     *  considers for scheduling to deal with unbounded queue length.
     */
    unsigned int get_queue_depth ()
    {
        return m_queue_depth;
    }

    /*! Look up a job whose jobid is id
     *
     *  \param id        jobid of flux_jobid_t type.
     *  \return          a shared pointer pointing to the job on success;
     *                       nullptr on error. ENOENT: unknown id.
     */
    const std::shared_ptr<job_t> lookup (flux_jobid_t id)
    {
        std::shared_ptr<job_t> job = nullptr;
        if (m_jobs.find (id) == m_jobs.end ()) {
            errno = ENOENT;
            return job;
        }
        return m_jobs[id];
    }

    /*! Append a job into the internal running-job queue to reconstruct
     *  the queue state.
     *
     * \param h          Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module such as qmanager, it is expected
     *                   to be a pointer to a flux_t object.
     * \param job        shared pointer to a running job whose resource
     *                   state is being requested to be reconstructed.
     *                   job->schedule.R is the requested R.
     * \param ret_R      replied R (must be equal to job->schedule.R
     *                   if succeeded).
     *  \return          0 on success; -1 on error.
     *                       EINVAL: invalid argument.
     *                       ENOMEM: out of memory.
     *                       ERANGE: out of range request.
     *                       EPROTO: job->schedule.R doesn't comply.
     *                       ENOTSUP: job->schedule.R has unsupported feature.
     */
    int reconstruct (void *h, std::shared_ptr<job_t> job, std::string &R_out)
    {
        int rc = 0;
        if ((rc = reconstruct_resource (h, job, R_out)) < 0)
            return rc;
        return reconstruct_queue (job);
    }

    /* Query the first job from the pending job queue.
     * \return           a shared pointer pointing to a job_t object
     *                   on success; nullptr when the queue is empty.
     */
    std::shared_ptr<job_t> pending_begin ()
    {
        std::shared_ptr<job_t> job_p = nullptr;
        m_pending_iter = m_pending.begin ();
        if (m_pending_iter == m_pending.end ()) {
            m_iter_valid = false;
        } else {
            flux_jobid_t id = m_pending_iter->second;
            m_iter_valid = true;
            if (m_jobs.find (id) != m_jobs.end ())
                job_p = m_jobs[id];
        }
        return job_p;
    }

    /* Query the next job from the pending job queue.
     * \return           a shared pointer pointing to a job_t object
     *                   on success; nullptr when the queue is empty.
     */
    std::shared_ptr<job_t> pending_next ()
    {
        std::shared_ptr<job_t> job_p = nullptr;
        if (!m_iter_valid)
            goto ret;
        m_pending_iter++;
        if (m_pending_iter == m_pending.end ()) {
            m_iter_valid = false;
        } else {
            flux_jobid_t id = m_pending_iter->second;
            m_iter_valid = true;
            if (m_jobs.find (id) != m_jobs.end ())
                job_p = m_jobs[id];
        }
    ret:
        return job_p;
    }

    /*! Append a job into the internal pending-job queue.
     *  If succeeds, it changes the pending job queue state and thus
     *  this queue becomes "schedulable": i.e., is_schedulable()
     *  returns true;
     *
     *  \param pending_job
     *                   a shared pointer pointing to a job_t object.
     *  \return          0 on success; -1 on error.
     *                       EINVAL: invalid argument.
     */
    int insert (std::shared_ptr<job_t> job)
    {
        int rc = -1;
        if (job == nullptr || m_jobs.find (job->id) != m_jobs.end ()) {
            errno = EINVAL;
            goto out;
        }
        job->state = job_state_kind_t::PENDING;
        job->t_stamps.pending_ts = m_pq_cnt++;
        m_pending_provisional.emplace (job->get_key (), job->id);
        m_jobs.insert (std::pair<flux_jobid_t, std::shared_ptr<job_t>> (job->id, job));
        set_schedulability (true);
        rc = 0;
    out:
        return rc;
    }

    /*! Remove a job whose jobid is id from the pending or maybe_pending queues.
     * If succeeds, it changes the pending queue or resource state. This queue
     * becomes "schedulable" if pending job queue is not empty: i.e.,
     * is_schedulable() returns true;
     *
     *  \param id        jobid of flux_jobid_t type.
     *  \return          0 on success; -1 on error.
     *                       ENOENT: unknown id.
     */
    int remove_pending (job_t *job)
    {
        int rc = -1;

        if (!job || job->state != job_state_kind_t::PENDING) {
            errno = EINVAL;
            return rc;
        }

        job->t_stamps.canceled_ts = m_cancel_cnt++;
        // try to cancel current sched loop, if result is zero the loop is
        // cancelled and the cancellation can proceed immediately
        if (is_sched_loop_active () && cancel_sched_loop () < 0) {
            // if sched-loop is active, the job's pending state
            // cannot be determined. There is "MAYBE pending state" where
            // a request has been sent out to the match service.
            auto res = m_pending_cancel_provisional.insert (
                std::pair<uint64_t, flux_jobid_t> (job->t_stamps.canceled_ts, job->id));
            if (!res.second) {
                errno = EEXIST;
                goto out;
            }
        } else {
            bool found_in_provisional = false;
            if (erase_pending_job (job, found_in_provisional) < 0)
                goto out;
            job->state = job_state_kind_t::CANCELED;
            auto res = m_canceled.insert (
                std::pair<uint64_t, flux_jobid_t> (job->t_stamps.canceled_ts, job->id));
            if (!res.second) {
                errno = EEXIST;
                goto out;
            }
            set_schedulability (true);
        }
        rc = 0;
    out:
        return rc;
    }

    /*! Remove a job whose jobid is id from any internal queues
     *  (e.g., pending queue, running queue, and alloced queue.)
     *  If succeeds, it changes the pending queue or resource
     *  state. This queue becomes "schedulable" if pending job
     *  queue is not empty: i.e., is_schedulable() returns true;
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module such as qmanager, it is expected
     *                   to be a pointer to a flux_t object.
     *  \param id        jobid of flux_jobid_t type.
     *  \param final     bool indicating if this is the final partial release RPC
     *                   for this jobid.
     *  \param R         Resource set for partial cancel
     *  \return          0 on success; -1 on error.
     *                       ENOENT: unknown id.
     */
    int remove (void *h, flux_jobid_t id, bool final, const char *R)
    {
        int rc = -1;
        bool full_removal = false;
        flux_t *flux_h = static_cast<flux_t *> (h);

        auto job_it = m_jobs.find (id);
        if (job_it == m_jobs.end ()) {
            errno = ENOENT;
            goto out;
        }

        switch (job_it->second->state) {
            case job_state_kind_t::PENDING:
                this->remove_pending (job_it->second.get ());
                break;
            case job_state_kind_t::ALLOC_RUNNING:
                // deliberately fall through
            case job_state_kind_t::RUNNING:
                if (!final) {
                    if (cancel (h, job_it->second->id, R, true, full_removal) != 0) {
                        flux_log_error (flux_h,
                                        "%s: .free RPC partial cancel failed for jobid "
                                        "%jd",
                                        __FUNCTION__,
                                        static_cast<intmax_t> (id));
                        errno = EINVAL;
                        goto out;
                    }
                } else {
                    // Run a full cancel to clean up all remaining allocated resources
                    if (cancel (h, job_it->second->id, true) != 0) {
                        flux_log_error (flux_h,
                                        "%s: .free RPC full cancel failed for jobid "
                                        "%jd",
                                        __FUNCTION__,
                                        static_cast<intmax_t> (id));
                        errno = EPROTO;
                        goto out;
                    }
                    full_removal = true;
                }
                // We still want to run the sched loop even if there's an inconsistent state
                set_schedulability (true);
                if (full_removal || final) {
                    m_alloced.erase (job_it->second->t_stamps.running_ts);
                    m_running.erase (job_it->second->t_stamps.running_ts);
                    job_it->second->t_stamps.complete_ts = m_cq_cnt++;
                    job_it->second->state = job_state_kind_t::COMPLETE;
                    // hold a reference to the shared_ptr to keep it alive
                    // during cancel
                    m_jobs.erase (job_it);
                    if (full_removal && !final) {
                        // This error condition can indicate a discrepancy between core and sched,
                        // specifically that a partial cancel removed an allocation prior to
                        // receiving the final .free RPC from core.
                        flux_log_error (flux_h,
                                        "%s: removed allocation before final .free RPC for "
                                        "jobid "
                                        "%jd",
                                        __FUNCTION__,
                                        static_cast<intmax_t> (id));
                        errno = EPROTO;
                        goto out;
                    }
                }
                break;
            default:
                break;
        }

        rc = 0;
    out:
        cancel_sched_loop ();
        // blocked jobs must be reconsidered after a job completes
        // this covers cases where jobs that couldn't run because of an
        // existing job's reservation can when it completes early
        reconsider_blocked_jobs ();
        return rc;
    }

    /*! Remove a job whose jobid is id from any internal queues
     *  (e.g., pending queue, running queue, and alloced queue.)
     *  If succeeds, it changes the pending queue or resource
     *  state. This queue becomes "schedulable" if pending job
     *  queue is not empty: i.e., is_schedulable() returns true;
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module such as qmanager, it is expected
     *                   to be a pointer to a flux_t object.
     *  \param id        jobid of flux_jobid_t type.
     *  \param R         Resource set for partial cancel
     *  \param noent_ok  don't return an error on nonexistent jobid
     *  \param full_removal  bool indicating whether the job is fully canceled
     *  \return          0 on success; -1 on error.
     *                       ENOENT: unknown id.
     */
    virtual int cancel (void *h, flux_jobid_t id, const char *R, bool noent_ok, bool &full_removal)
    {
        full_removal = true;
        return 0;
    }

    /*! Remove a job whose jobid is id from any internal queues
     *  (e.g., pending queue, running queue, and alloced queue.)
     *  If succeeds, it changes the pending queue or resource
     *  state. This queue becomes "schedulable" if pending job
     *  queue is not empty: i.e., is_schedulable() returns true;
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module such as qmanager, it is expected
     *                   to be a pointer to a flux_t object.
     *  \param id        jobid of flux_jobid_t type.
     *  \param noent_ok  don't return an error on nonexistent jobid
     *  \return          0 on success; -1 on error.
     *                       ENOENT: unknown id.
     */
    virtual int cancel (void *h, flux_jobid_t id, bool noent_ok)
    {
        return 0;
    }

    /*! Return true if this queue has become schedulable since
     *  its state had been reset with set_schedulability (false).
     *  "Being schedulable" means one or more job or resource events
     *  have occurred such a way that the scheduler should run the
     *  scheduling loop for the pending jobs: e.g., a new job was
     *  inserted into the pending job queue or a job was removed from
     *  the running job queue so that its resource was released.
     */
    bool is_schedulable ()
    {
        return m_schedulable;
    }

    /*! Set this queue's schedulability. After this call,
     *  is_schedulable() will return the newly set schedulability
     *  until a new job or resource event occurs.
     */
    void set_schedulability (bool schedulable)
    {
        m_schedulable = schedulable;
    }

    /*! Return true if the job state of this queue has changed
     *  as the result of the invocation of schedule loop and
     *  and/or of other conditions.
     */
    bool is_scheduled ()
    {
        return m_scheduled;
    }

    /*! Reset this queue's "scheduled" state.
     */
    void reset_scheduled ()
    {
        m_scheduled = false;
    }

    /*! Implement queue_adapter_base_t's pure virtual method
     *  so that this queue can be adapted for use within high-level
     *  resource API. Return true if the scheduling loop is active.
     */
    bool is_sched_loop_active () override
    {
        return m_sched_loop_active;
    }

    /*! Implement queue_adapter_base_t's pure virtual method
     *  so that this queue can be adapted for use within high-level
     *  resource API. Set the state of the scheduling loop.
     *  \param active    true when the scheduling loop becomes
     *                   active; false when becomes inactive.
     *  \return          0 on success; otherwise -1 an error with errno set
     *                   (Note: when the scheduling loop becomes inactive,
     *                    internal queueing can occur and an error can arise):
     *                       - ENOENT (job is not found from some queue)
     *                       - EEXIST (enqueue fails due to an existent entry)
     */
    int set_sched_loop_active (bool active) override
    {
        int rc = 0;
        bool prev = m_sched_loop_active;
        m_sched_loop_active = active;
        if (prev && !m_sched_loop_active) {
            rc = process_provisional_reprio ();
            rc += process_provisional_cancel ();
            process_provisional_reconsider ();
        }
        return rc;
    }

    /*! Implement queue_adapter_base_t's pure virtual method
     *  so that this queue can be adapted for use within high-level
     *  resource API. When a match succeeds, this method is called back
     *  by reapi_t.
     */
    int handle_match_success (flux_jobid_t jobid,
                              const char *status,
                              const char *R,
                              int64_t at,
                              double ov) override
    {
        return 0;
    }

    /*! Implement queue_adapter_base_t's pure virtual method
     *  so that this queue can be adapted for use within high-level
     *  resource API. When a match fails, this method is called back
     *  by reapi_t.
     */
    int handle_match_failure (flux_jobid_t jobid, int errcode) override
    {
        return 0;
    }

    /*! Pop the first job from the pending job queue. The popped
     *  job is completely graduated from the queue policy layer.
     *
     *  \return          a shared pointer pointing to a job_t object
     *                   on success; nullptr when the queue is empty.
     */
    std::shared_ptr<job_t> pending_pop ()
    {
        std::shared_ptr<job_t> job;
        flux_jobid_t id;

        if (m_pending.empty ())
            return nullptr;
        id = m_pending.begin ()->second;
        if (m_jobs.find (id) == m_jobs.end ())
            return nullptr;
        job = m_jobs[id];
        m_pending.erase (job->get_key ());
        m_jobs.erase (id);
        return job;
    }

    /*! Pop the first job from the alloced job queue. The popped
     *  job still remains in the queue policy layer (i.e., in the
     *  internal running job queue).
     *  \return          a shared pointer pointing to a job_t object
     *                   on success; nullptr when the queue is empty.
     */
    std::shared_ptr<job_t> alloced_pop ()
    {
        std::shared_ptr<job_t> job;
        flux_jobid_t id;
        if (m_alloced.empty ())
            return nullptr;
        id = m_alloced.begin ()->second;
        if (m_jobs.find (id) == m_jobs.end ())
            return nullptr;
        job = m_jobs[id];
        m_alloced.erase (job->t_stamps.running_ts);
        return job;
    }

    /*! Pop the first job from the rejected job queue.
     *  The popped is completely graduated from the queue policy layer.
     *  \return          a shared pointer pointing to a job_t object
     *                   on success; nullptr when the queue is empty.
     */
    std::shared_ptr<job_t> rejected_pop ()
    {
        std::shared_ptr<job_t> job;
        flux_jobid_t id;
        if (m_rejected.empty ())
            return nullptr;
        id = m_rejected.begin ()->second;
        if (m_jobs.find (id) == m_jobs.end ())
            return nullptr;
        job = m_jobs[id];
        m_rejected.erase (job->t_stamps.rejected_ts);
        return job;
    }

    /*! Pop the first job from the internal canceled job queue.
     *  The popped is completely graduated from the queue policy layer.
     *  \return          a shared pointer pointing to a job_t object
     *                   on success; nullptr when the queue is empty.
     */
    std::shared_ptr<job_t> canceled_pop ()
    {
        std::shared_ptr<job_t> job;
        flux_jobid_t id;
        if (m_canceled.empty ())
            return nullptr;
        id = m_canceled.begin ()->second;
        if (m_jobs.find (id) == m_jobs.end ())
            return nullptr;
        job = m_jobs[id];
        m_canceled.erase (job->t_stamps.canceled_ts);
        m_jobs.erase (id);
        return job;
    }

    /* This doesn't appear to be used anywhere */
    std::shared_ptr<job_t> reserved_pop ();

    job_map_iter to_running (job_map_iter pending_iter, bool use_alloced_queue)
    {
        flux_jobid_t id = pending_iter->second;
        if (m_jobs.find (id) == m_jobs.end ()) {
            errno = EINVAL;
            return pending_iter;
        }

        std::shared_ptr<job_t> job = m_jobs[id];
        job->state = job_state_kind_t::RUNNING;
        job->t_stamps.running_ts = m_rq_cnt++;
        auto res = m_running.insert (
            std::pair<uint64_t, flux_jobid_t> (job->t_stamps.running_ts, job->id));
        if (!res.second) {
            errno = ENOMEM;
            return pending_iter;
        }

        if (use_alloced_queue) {
            job->state = job_state_kind_t::ALLOC_RUNNING;
            auto res = m_alloced.insert (
                std::pair<uint64_t, flux_jobid_t> (job->t_stamps.running_ts, job->id));
            if (!res.second) {
                errno = ENOMEM;
                return pending_iter;
            }
            m_scheduled = true;
        }
        // Return the next iterator after pending_iter. This way,
        // the upper layer can modify m_pending while iterating the queue
        return m_pending.erase (pending_iter);
    }

    /*! Reprioritize a job with a new priority.
     *
     *  \param id        jobid of flux_jobid_t type.
     *  \param priority  new job priority
     *  \return          0 on success; -1 on error.
     *                       ENOENT: unknown id.
     *                       EEXIST: id already exists
     *                       EINVAL: job not pending
     */
    int pending_reprioritize (flux_jobid_t id, unsigned int priority)
    {
        std::shared_ptr<job_t> job = nullptr;

        if (m_jobs.find (id) == m_jobs.end ()) {
            errno = ENOENT;
            return -1;
        }
        job = m_jobs[id];
        if (job->state != job_state_kind_t::PENDING) {
            errno = EINVAL;
            return -1;
        }

        auto res = m_pending_reprio_provisional.insert (
            std::pair<uint64_t, std::pair<flux_jobid_t, unsigned int>> (m_reprio_cnt,
                                                                        std::make_pair (job->id,
                                                                                        priority)));
        m_reprio_cnt++;
        if (!res.second) {
            errno = EEXIST;
            return -1;
        }
        if (!is_sched_loop_active () || cancel_sched_loop () == 0) {
            return process_provisional_reprio ();
        }
        return 0;
    }

   protected:
    /*! Reconstruct the queue.
     *
     * \param job        shared pointer to a running job whose resource
     *                   state is being requested to be reconstructed.
     *                   job->schedule.R is the requested R.
     *  \return          0 on success; -1 on error.
     *                       EINVAL: invalid argument.
     *                       ENOMEM: out of memory.
     *                       ERANGE: out of range request.
     *                       EPROTO: job->schedule.R doesn't comply.
     *                       ENOTSUP: job->schedule.R has unsupported feature.
     */
    int reconstruct_queue (std::shared_ptr<job_t> job)
    {
        int rc = -1;
        std::pair<std::map<uint64_t, flux_jobid_t>::iterator, bool> ret;
        std::pair<std::map<flux_jobid_t, std::shared_ptr<job_t>>::iterator, bool> ret2;

        if (job == nullptr || m_jobs.find (job->id) != m_jobs.end ()) {
            errno = EINVAL;
            goto out;
        }
        job->t_stamps.running_ts = m_rq_cnt++;

        ret = m_running.insert (
            std::pair<uint64_t, flux_jobid_t> (job->t_stamps.running_ts, job->id));
        if (ret.second == false) {
            rc = -1;
            errno = ENOMEM;
            goto out;
        }
        ret2 = m_jobs.insert (std::pair<flux_jobid_t, std::shared_ptr<job_t>> (job->id, job));
        if (ret2.second == false) {
            m_running.erase (ret.first);
            rc = -1;
            errno = ENOMEM;
            goto out;
        }

        rc = 0;
    out:
        return rc;
    }

    int process_provisional_cancel ()
    {
        for (auto kv : m_pending_cancel_provisional) {
            auto id = kv.second;
            if (m_jobs.find (id) == m_jobs.end ()) {
                errno = ENOENT;
                return -1;
            }
            auto job = m_jobs[id];
            if (job->state == job_state_kind_t::PENDING) {
                bool found_in_provisional = false;
                if (erase_pending_job (job.get (), found_in_provisional) < 0)
                    return -1;
                job->state = job_state_kind_t::CANCELED;
                auto res = m_canceled.insert (
                    std::pair<uint64_t, flux_jobid_t> (job->t_stamps.canceled_ts, job->id));
                if (!res.second) {
                    errno = EEXIST;
                    return -1;
                }
                m_schedulable = true;
            }
        }
        m_pending_cancel_provisional.clear ();
        return 0;
    }

    int process_provisional_reprio ()
    {
        for (auto kv : m_pending_reprio_provisional) {
            auto res = kv.second;
            auto id = res.first;
            auto priority = res.second;
            if (m_jobs.find (id) == m_jobs.end ()) {
                errno = ENOENT;
                return -1;
            }
            auto job = m_jobs[id];
            if (job->state == job_state_kind_t::PENDING) {
                bool found_in_provisional = false;
                if (erase_pending_job (job.get (), found_in_provisional) < 0)
                    return -1;
                job->priority = priority;
                if (insert_pending_job (job, found_in_provisional) < 0)
                    return -1;
                set_schedulability (true);
                // in case this job is now lower priority than one it was blocking,
                // reconsider blocked jobs
                reconsider_blocked_jobs ();
            }
        }
        m_pending_reprio_provisional.clear ();
        return 0;
    }

    int insert_pending_job (std::shared_ptr<job_t> &job, bool into_provisional)
    {
        auto &pending_map = into_provisional ? m_pending_provisional : m_pending;
        auto res = pending_map.emplace (job->get_key (), job->id);
        if (!res.second) {
            errno = EEXIST;
            return -1;
        }
        return 0;
    }

    int erase_pending_job (job_t *job, bool &found_in_prov)
    {
        size_t s;
        s = m_pending.erase (job->get_key ());
        if (s == 1) {
            return 0;
        }
        s = m_blocked.erase (job->get_key ());
        if (s == 1) {
            return 0;
        }
        // job must be in m_pending_provisional in this case
        s = m_pending_provisional.erase (job->get_key ());
        if (s == 0) {
            errno = ENOENT;
            return -1;
        }
        found_in_prov = true;
        return 0;
    }

    job_map_iter to_rejected (job_map_iter pending_iter, const std::string &note)
    {
        flux_jobid_t id = pending_iter->second;
        if (m_jobs.find (id) == m_jobs.end ()) {
            errno = EINVAL;
            return pending_iter;
        }

        std::shared_ptr<job_t> job = m_jobs[id];
        job->state = job_state_kind_t::REJECTED;
        job->note = note;
        job->t_stamps.rejected_ts = m_dq_cnt++;
        auto res = m_rejected.insert (
            std::pair<uint64_t, flux_jobid_t> (job->t_stamps.rejected_ts, job->id));
        if (!res.second) {
            errno = ENOMEM;
            return pending_iter;
        }
        m_scheduled = true;
        // Return the next iterator after pending_iter. This way,
        // the upper layer can modify m_pending while iterating the queue
        return m_pending.erase (pending_iter);
    }

    bool m_schedulable = false;
    bool m_scheduled = false;
    bool m_sched_loop_active = false;
    uint64_t m_pq_cnt = 0;
    uint64_t m_rq_cnt = 0;
    uint64_t m_dq_cnt = 0;
    uint64_t m_cq_cnt = 0;
    uint64_t m_oq_cnt = 0;
    uint64_t m_cancel_cnt = 0;
    uint64_t m_reprio_cnt = 0;
    unsigned int m_queue_depth = DEFAULT_QUEUE_DEPTH;
    unsigned int m_max_queue_depth = MAX_QUEUE_DEPTH;
    /// jobs that need to wait for resource state updates
    std::map<pending_key, flux_jobid_t> m_blocked;
    std::map<pending_key, flux_jobid_t> m_pending;
    std::map<pending_key, flux_jobid_t> m_pending_provisional;
    std::map<uint64_t, flux_jobid_t> m_pending_cancel_provisional;
    bool m_pending_reconsider = false;
    std::map<uint64_t, std::pair<flux_jobid_t, unsigned int>> m_pending_reprio_provisional;
    std::map<uint64_t, flux_jobid_t> m_running;
    std::map<uint64_t, flux_jobid_t> m_alloced;
    std::map<uint64_t, flux_jobid_t> m_rejected;
    std::map<uint64_t, flux_jobid_t> m_canceled;
    std::map<flux_jobid_t, std::shared_ptr<job_t>> m_jobs;
    std::unordered_map<std::string, std::string> m_qparams;
    std::unordered_map<std::string, std::string> m_pparams;

   private:
    int set_params (const std::string &params, std::unordered_map<std::string, std::string> &p_map)
    {
        int rc = -1;
        size_t pos = 0;
        std::string p_copy = params;
        std::string delim = ",";

        try {
            while ((pos = p_copy.find (delim)) != std::string::npos) {
                std::string p_pair = p_copy.substr (0, pos);
                if (set_param (p_pair, p_map) < 0)
                    goto done;
                p_copy.erase (0, pos + delim.length ());
            }
            if (set_param (p_copy, p_map) < 0)
                goto done;
            rc = 0;
        } catch (std::out_of_range &e) {
            errno = EINVAL;
            rc = -1;
        } catch (std::bad_alloc &e) {
            errno = ENOMEM;
            rc = -1;
        }
    done:
        return rc;
    }

    int set_param (std::string &p_pair, std::unordered_map<std::string, std::string> &p_map)
    {
        int rc = -1;
        size_t pos = 0;
        std::string k, v;
        std::string split = "=";

        if ((pos = p_pair.find (split)) == std::string::npos) {
            errno = EINVAL;
            goto done;
        }
        k = p_pair.substr (0, pos);
        k.erase (std::remove_if (k.begin (), k.end (), ::isspace), k.end ());
        if (k.empty ()) {
            errno = EINVAL;
            goto done;
        }
        v = p_pair.erase (0, pos + split.length ());
        v.erase (std::remove_if (v.begin (), v.end (), ::isspace), v.end ());
        if (p_map.find (k) != p_map.end ())
            p_map.erase (k);
        p_map.insert (std::pair<std::string, std::string> (k, v));
        rc = 0;
    done:
        return rc;
    }

    bool is_number (const std::string &num_str)
    {
        if (num_str.empty ())
            return false;
        auto i = std::find_if (num_str.begin (), num_str.end (), [] (unsigned char c) {
            return !std::isdigit (c);
        });
        return i == num_str.end ();
    }

    job_map_iter m_pending_iter;
    bool m_iter_valid = false;
};

}  // namespace queue_manager
}  // namespace Flux

#endif  // QUEUE_POLICY_BASE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
