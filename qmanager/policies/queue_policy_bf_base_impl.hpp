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

namespace Flux {
namespace queue_manager {
namespace detail {


////////////////////////////////////////////////////////////////////////////////
// Private Methods of Queue Policy Backfill Base
////////////////////////////////////////////////////////////////////////////////

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::cancel_completed_jobs (void *h)
{
    int rc = 0;
    std::shared_ptr<job_t> job;

    // Pop newly completed jobs (e.g., per a free request from
    // job-manager as received by qmanager) to remove them from the
    // resource infrastructure.
    while ((job = complete_pop ()) != nullptr)
        rc += reapi_type::cancel (h, job->id, true);
    return rc;
}

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

template <class reapi_type>
void queue_policy_bf_base_t<reapi_type>::init_sched_loop () {
    set_sched_loop_active (false);

    // move jobs in m_pending_provisional queue into
    // m_pending.
    m_pending.merge (m_pending_provisional);
    m_pending_provisional.clear ();

    // reset our iterator to the start
    m_in_progress_iter = m_pending.begin ();
    m_reservation_cnt = 0;
    m_scheduled_cnt = 0;
}

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::allocate_orelse_reserve_jobs (void *h,
                                                                      bool use_alloced_queue)
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
        init_sched_loop ();
        set_sched_loop_active (true);
    }

    int saved_errno = errno;
    for (int i = 0; (m_in_progress_iter != m_pending.end ()) && (m_scheduled_cnt < m_queue_depth);
         ++m_scheduled_cnt, ++i) {
        if (i > 5) {
            // set schedulable so that the callback machinery gets us
            // back in with prep and check
            set_schedulability (true);
            errno = EAGAIN;
            return -1;
        }
        errno = 0;
        auto &job = m_jobs[m_in_progress_iter->second];
        bool try_reserve = m_reservation_cnt < m_reservation_depth;
        int64_t at = job->schedule.at;
        if (!reapi_type::match_allocate (h,
                                         try_reserve,
                                         job->jobspec,
                                         job->id,
                                         job->schedule.reserved,
                                         job->schedule.R,
                                         job->schedule.at,
                                         job->schedule.ov)) {
            if (job->schedule.reserved) {
                // High-priority job has been reserved, continue
                m_reserved.insert (std::pair<uint64_t, flux_jobid_t> (m_oq_cnt++, job->id));
                job->schedule.old_at = at;
                m_reservation_cnt++;
                m_in_progress_iter++;
            } else {
                // move the job to the running queue and make sure the
                // job is enqueued into allocated job queue as well.
                // When this is used within a module, it allows the
                // module to fetch those newly allocated jobs, which
                // have flux_msg_t to respond to job-manager.
                m_in_progress_iter = to_running (m_in_progress_iter, use_alloced_queue);
            }
        } else if (errno != EBUSY) {
            // The request must be rejected. The job is enqueued into
            // rejected job queue to the upper layer to react on this.
            m_in_progress_iter = to_rejected (m_in_progress_iter,
                                              (errno == ENODEV) ? "unsatisfiable" : "match error");
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
            if (try_reserve) {
                m_blocked.insert (m_pending.extract (element_iter));
                // avoid counting this toward queue_depth
                --m_scheduled_cnt;
            }
        }
    }
    set_sched_loop_active (false);
    errno = saved_errno;
    return 0; 
}


////////////////////////////////////////////////////////////////////////////////
// Public API of Queue Policy Backfill Base
////////////////////////////////////////////////////////////////////////////////

template<class reapi_type>
queue_policy_bf_base_t<reapi_type>::~queue_policy_bf_base_t ()
{
}

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::apply_params ()
{
    return 0;
}

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::run_sched_loop (void *h, bool use_alloced_queue)
{
    int rc = 0;
    set_schedulability (false);
    if (!is_sched_loop_active ()) {
        rc = cancel_completed_jobs (h);
        if (rc != 0)
            return rc;
        rc = cancel_reserved_jobs (h);
        if (rc != 0)
            return rc;
    }
    return allocate_orelse_reserve_jobs (h, use_alloced_queue);
}

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::cancel_sched_loop ()
{
    int rc = 0;
    if (!is_sched_loop_active ()) {
        errno = EINVAL;
        return -1;
    }
    init_sched_loop ();
    set_schedulability (true);
    return 0;
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
