/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef QUEUE_POLICY_BF_BASE_IMPL_HPP
#define QUEUE_POLICY_BF_BASE_IMPL_HPP

#include "qmanager/policies/queue_policy_bf_base.hpp"
#include <jansson.h>
#include <memory>

namespace Flux {
namespace queue_manager {
namespace detail {

////////////////////////////////////////////////////////////////////////////////
// Private Methods of Queue Policy Backfill Base
////////////////////////////////////////////////////////////////////////////////

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::cancel_reserved_jobs (void *h)
{
    int rc = 0;
    std::map<uint64_t, flux_jobid_t>::const_iterator citer;
    for (citer = m_reserved.begin (); citer != m_reserved.end (); citer++)
        rc += reapi_type::cancel (h, citer->second, false);
    m_reserved.clear ();
    return rc;
}

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::next_match_iter ()
{
    bool reached_queue_depth = m_scheduled_cnt >= m_queue_depth;
    bool reached_end_of_queue = m_in_progress_iter == m_pending.end ();
    if (reached_end_of_queue || reached_queue_depth) {
        // we're at the end, clean up
        set_sched_loop_active (false);
        if (reached_queue_depth && !reached_end_of_queue && is_scheduled ()) {
            // there are more jobs to schedule, and we scheduled something
            // successfully, start the loop over
            set_schedulability (true);
        }
        return 0;
    }

    auto &job = m_jobs[m_in_progress_iter->second];
    m_try_reserve = m_reservation_cnt < m_reservation_depth;
    json_t *jobarray_ptr =
        json_pack ("[{s:I s:s}]", "jobid", job->id, "jobspec", job->jobspec.c_str ());
    if (!jobarray_ptr) {
        errno = ENOMEM;
        return -1;
    }
    auto _jdecref = [] (json_t *p) { json_decref (p); };
    std::unique_ptr<json_t, decltype (_jdecref)> jobarray (jobarray_ptr, _jdecref);
    char *jobs_ptr = json_dumps (jobarray.get (), JSON_INDENT (0));
    if (!jobs_ptr) {
        errno = ENOMEM;
        return -1;
    }
    auto _free = [] (char *p) { free (p); };
    std::unique_ptr<char, decltype (_free)> jobs_str (jobs_ptr, _free);
    return reapi_type::match_allocate_multi (m_handle, m_try_reserve, jobs_str.get (), this);
}

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::allocate_orelse_reserve_jobs (void *h)
{
    // Iterate jobs in the pending job queue and try to allocate each
    // until you can't. When you can't allocate a job, you reserve it
    // and then try to backfill later jobs.
    //
    // In order to avoid long blocking, this is a resumable sched loop,
    // which stores its iteration progress in m_in_progress_iter and
    // returns after processing only _some_ of the work. In that case,
    // it currently returns -1 and EAGAIN to indicate that it _is not
    // done_ and the qmanager callbacks should re-enter the scheduling
    // loop in the next check phase.
    if (!this->is_sched_loop_active ()) {
        // move jobs in m_pending_provisional queue into
        // m_pending.
        m_pending.merge (m_pending_provisional);
        m_pending_provisional.clear ();

        // reset our iterator to the start
        m_in_progress_iter = m_pending.begin ();
        m_reservation_cnt = 0;
        m_scheduled_cnt = 0;

        set_sched_loop_active (true);

        m_handle = h;
        // now that we've initialized, start the chain
        return next_match_iter ();
    }
    return 0;
}

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::cancel (void *h,
                                                flux_jobid_t id,
                                                const char *R,
                                                bool noent_ok,
                                                bool &full_removal)
{
    return reapi_type::cancel (h, id, R, noent_ok, full_removal);
}

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::cancel (void *h, flux_jobid_t id, bool noent_ok)
{
    return reapi_type::cancel (h, id, noent_ok);
}

////////////////////////////////////////////////////////////////////////////////
// Public API of Queue Policy Backfill Base
////////////////////////////////////////////////////////////////////////////////

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::apply_params ()
{
    return 0;
}

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::handle_match_success (flux_jobid_t jobid,
                                                              const char *status,
                                                              const char *R,
                                                              int64_t at,
                                                              double ov)
{
    if (!is_sched_loop_active ()) {
        errno = EINVAL;
        return -1;
    }
    auto job_it = m_jobs.find (m_in_progress_iter->second);
    if (job_it == m_jobs.end ()) {
        // The job is just missing, much worse
        errno = ENOENT;
        return -1;
    }
    auto &job = job_it->second;
    if (job->id != static_cast<flux_jobid_t> (jobid)) {
        errno = EINVAL;
        return -1;
    }
    auto &sched = job->schedule;
    sched.reserved = std::string ("RESERVED") == status ? true : false;
    sched.R = R;
    sched.old_at = sched.at;
    sched.at = at;
    sched.ov = ov;
    if (job->schedule.reserved) {
        // High-priority job has been reserved, continue
        m_reserved.insert (std::pair<uint64_t, flux_jobid_t> (m_oq_cnt++, job->id));
        m_reservation_cnt++;
        m_in_progress_iter++;
        // reply with an annotation
        m_scheduled = true;
    } else {
        // move the job to the running queue and make sure the
        // job is enqueued into allocated job queue as well.
        // When this is used within a module, it allows the
        // module to fetch those newly allocated jobs, which
        // have flux_msg_t to respond to job-manager.
        m_in_progress_iter = to_running (m_in_progress_iter, true);
    }
    ++m_scheduled_cnt;
    return next_match_iter ();
}

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::handle_match_failure (flux_jobid_t jobid, int errcode)
{
    if (!is_sched_loop_active ()) {
        errno = EINVAL;
        return -1;
    }
    errno = errcode;
    if (errno == ENODATA) {
        // end of sequence, do nothing with this, at all
        return 0;
    } else if (errno != EBUSY) {
        // The request must be rejected. The job is enqueued into
        // rejected job queue to the upper layer to react on this.
        m_in_progress_iter =
            to_rejected (m_in_progress_iter, (errno == ENODEV) ? "unsatisfiable" : "match error");
    } else {
        // errno is EBUSY and match_allocate returned -1

        // copy iterator before we advance to avoid invalidation
        auto element_iter = m_in_progress_iter;
        // if we are allocating and not trying to reserve, as in FCFS for
        // example, EBUSY means the request failed because not enough
        // resources are available right now.

        // Regardless we want to move to the next item in the map
        ++m_in_progress_iter;

        // If we are trying to reserve, then EBUSY means not only can it not
        // be allocated, but it cannot ever be reserved given current
        // conditions. This can happen if there are "down" resources.
        // The semantics of our backfill policies are to skip this
        // job, add it to the blocked list, and re-consider when
        // resource status changes
        if (m_try_reserve) {
            m_blocked.insert (m_pending.extract (element_iter));
            // avoid counting this toward queue_depth
            --m_scheduled_cnt;
        }
    }
    ++m_scheduled_cnt;
    return next_match_iter ();
}

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::run_sched_loop (void *h, bool use_alloced_queue)
{
    int rc = 0;
    if (!is_sched_loop_active ()) {
        set_schedulability (false);
        rc = cancel_reserved_jobs (h);
        if (rc != 0)
            return rc;
    }
    return allocate_orelse_reserve_jobs (h);
}

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::cancel_sched_loop ()
{
    int rc = 0;
    if (!is_sched_loop_active ()) {
        // nothing to cancel, safe to proceed
        return 0;
    }
    m_scheduled_cnt = m_queue_depth;
    // we want the actual cancel deferred until the loop exits, it simplifies
    // the handling and coherence a great deal
    errno = 0;
    return -1;
}

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::reconstruct_resource (void *h,
                                                              std::shared_ptr<job_t> job,
                                                              std::string &R_out)
{
    return reapi_type::update_allocate (h,
                                        job->id,
                                        job->schedule.R,
                                        job->schedule.at,
                                        job->schedule.ov,
                                        R_out);
}

}  // namespace detail
}  // namespace queue_manager
}  // namespace Flux

#endif  // QUEUE_POLICY_BF_BASE_IMPL_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
