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

/******************************************************************************
 *                                                                            *
 *                 Private Methods of Queue Policy Backfill Base              *
 *                                                                            *
 ******************************************************************************/

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::cancel_completed_jobs (void *h)
{
    int rc = 0;
    std::shared_ptr<job_t> job;

    // Pop newly completed jobs (e.g., per a free request from job-manager
    // as received by qmanager) to remove them from the resource infrastructure.
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

template<class reapi_type>
std::map<std::vector<double>, flux_jobid_t>::iterator &
queue_policy_bf_base_t<reapi_type>::allocate_orelse_reserve (void *h,
                                               std::shared_ptr<job_t> job,
                                               bool use_alloced_queue,
                                               std::map<std::vector<double>,
                                                  flux_jobid_t>::iterator &iter)


{
    int64_t at = job->schedule.at;
    if (reapi_type::match_allocate (h, true, job->jobspec, job->id,
                                    job->schedule.reserved, job->schedule.R,
                                    job->schedule.at, job->schedule.ov) == 0) {

        if (job->schedule.reserved) {
            // High-priority job has been reserved, continue
            m_reserved.insert (std::pair<uint64_t, flux_jobid_t> (m_oq_cnt++,
                                                                  job->id));
	    job->schedule.old_at = at;
            m_reservation_cnt++;
            iter++;
        } else {
            // move the job to the running queue and make sure the job
            // is enqueued into allocated job queue as well.
            // When this is used within a module, it allows the module
            // to fetch those newly allocated jobs, which have flux_msg_t to
            // respond to job-manager.
            iter = to_running (iter, use_alloced_queue);
        }
    } else {
        if (errno != EBUSY) {
            // The request must be rejected. The job is enqueued into
            // rejected job queue to the upper layer to react on this.
            iter = to_rejected (iter, (errno == ENODEV)? "unsatisfiable"
                                                       : "match error");
        } else {
            // This can happen if there are "down" resources.
            // The semantics of our backfill policies is to skip this job
            iter++;
        }
    }
    return iter;
}

template<class reapi_type>
std::map<std::vector<double>, flux_jobid_t>::iterator &
queue_policy_bf_base_t<reapi_type>::allocate (void *h, std::shared_ptr<job_t> job,
                                              bool use_alloced_queue,
                                              std::map<std::vector<double>,
                                                  flux_jobid_t>::iterator &iter)
{
    if (reapi_type::match_allocate (h, false, job->jobspec, job->id,
                                    job->schedule.reserved, job->schedule.R,
                                    job->schedule.at, job->schedule.ov) == 0) {
        // move the job to the running queue and make sure the job
        // is enqueued into allocated job queue as well.
        // When this is used within a module, it allows the module
        // to fetch those newly allocated jobs, which have flux_msg_t to
        // respond to job-manager.
        iter = to_running (iter, use_alloced_queue);
    } else {
        if (errno != EBUSY) {
            // The request must be rejected. The job is enqueued into
            // rejected job queue to the upper layer to react on this.
            iter = to_rejected (iter, (errno == ENODEV)? "unsatisfiable"
                                                       : "match error");
        } else {
            iter++;
        }
    }
    return iter;
}

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::allocate_orelse_reserve_jobs (void *h,
                                            bool use_alloced_queue)
{
    unsigned int i = 0;
    std::shared_ptr<job_t> job;

    // move jobs in m_pending_provisional queue into
    // m_pending. Note that c++11 doesn't have a clean way
    // to "move" elements between two std::map objects so
    // we use copy for the time being.
    m_pending.insert (m_pending_provisional.begin (),
                      m_pending_provisional.end ());
    m_pending_provisional.clear ();

    set_sched_loop_active (true);

    // Iterate jobs in the pending job queue and try to allocate each
    // until you can't. When you can't allocate a job, you reserve it
    // and then try to backfill later jobs.
    std::map<std::vector<double>, flux_jobid_t>::iterator iter
        = m_pending.begin ();
    m_reservation_cnt = 0;
    int saved_errno = errno;
    while ((iter != m_pending.end ()) && (i < m_queue_depth)) {
        errno = 0;
        job = m_jobs[iter->second];
        if (m_reservation_cnt < m_reservation_depth)
            iter = allocate_orelse_reserve (h, job, use_alloced_queue, iter);
        else
            iter = allocate (h, job, use_alloced_queue, iter);
       i++;
    }
    set_sched_loop_active (false);
    errno = saved_errno;
    return 0;
}


/******************************************************************************
 *                                                                            *
 *                 Public API of Queue Policy Backfill Base                   *
 *                                                                            *
 ******************************************************************************/

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
int queue_policy_bf_base_t<reapi_type>::run_sched_loop (void *h,
                                                        bool use_alloced_queue)
{
    int rc = 0;
    set_schedulability (false);
    rc = cancel_completed_jobs (h);
    rc += cancel_reserved_jobs (h);
    rc += allocate_orelse_reserve_jobs (h, use_alloced_queue);
    return rc;
}

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::reconstruct_resource (
        void *h, std::shared_ptr< job_t> job, std::string &R_out)
{
    return reapi_type::update_allocate (h, job->id, job->schedule.R,
                                        job->schedule.at,
                                        job->schedule.ov, R_out);
}

} // namespace Flux::queue_manager::detail
} // namespace Flux::queue_manager
} // namespace Flux

#endif // QUEUE_POLICY_BF_BASE_IMPL_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
