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
        rc += reapi_type::cancel (h, job->id);
    return rc;
}

template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::cancel_reserved_jobs (void *h)
{
    int rc = 0;
    std::map<uint64_t, flux_jobid_t>::const_iterator citer;
    for (citer = m_reserved.begin (); citer != m_reserved.end (); citer++)
        rc += reapi_type::cancel (h, citer->second);
    m_reserved.clear ();
    return rc;
}

template<class reapi_type>
std::map<uint64_t, flux_jobid_t>::iterator &
queue_policy_bf_base_t<reapi_type>::allocate_orelse_reserve (void *h,
                                               std::shared_ptr<job_t> job,
                                               bool use_alloced_queue,
                                               std::map<uint64_t,
                                                  flux_jobid_t>::iterator &iter)


{
    if (reapi_type::match_allocate (h, true, job->jobspec, job->id,
                                    job->schedule.reserved, job->schedule.R,
                                    job->schedule.at, job->schedule.ov) == 0) {

        if (job->schedule.reserved) {
            // High-priority job has been reserved, continue
            m_reserved.insert (std::pair<uint64_t, flux_jobid_t> (m_oq_cnt++,
                                                                  job->id));
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
        // The request must be rejected. The job is enqueued into
        // rejected job queue to the upper layer to react on this.
        iter = to_rejected (iter, (errno == ENODEV)? "unsatisfiable"
                                                   : "match error");
    }
    return iter;
}

template<class reapi_type>
std::map<uint64_t, flux_jobid_t>::iterator &
queue_policy_bf_base_t<reapi_type>::allocate (void *h, std::shared_ptr<job_t> job,
                                              bool use_alloced_queue,
                                              std::map<uint64_t,
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

    // Iterate jobs in the pending job queue and try to allocate each
    // until you can't. When you can't allocate a job, you reserve it
    // and then try to backfill later jobs.
    std::map<uint64_t, flux_jobid_t>::iterator iter = m_pending.begin ();
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
    int rc = -1;
    try {
        std::unordered_map<std::string, std::string>::const_iterator i;
        if ((i = queue_policy_base_impl_t::m_params.find ("queue-depth"))
             != queue_policy_base_impl_t::m_params.end ()) {
            unsigned int depth = std::stoi (i->second);
            if (depth < MAX_QUEUE_DEPTH)
                queue_policy_base_impl_t::m_queue_depth = depth;
        }
        rc = 0;
    } catch (const std::invalid_argument &e) {
        errno = EINVAL;
    } catch (const std::out_of_range &e) {
        errno = ERANGE;
    }
    return rc;
}


template<class reapi_type>
int queue_policy_bf_base_t<reapi_type>::run_sched_loop (void *h,
                                                        bool use_alloced_queue)
{
    int rc = 0;
    rc = cancel_completed_jobs (h);
    rc += allocate_orelse_reserve_jobs (h, use_alloced_queue);
    rc += cancel_reserved_jobs (h);
    return rc;
}

} // namespace Flux::queue_manager::detail
} // namespace Flux::queue_manager
} // namespace Flux

#endif // QUEUE_POLICY_BF_BASE_IMPL_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
