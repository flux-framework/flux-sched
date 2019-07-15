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

#ifndef QUEUE_POLICY_BASE_IMPL_HPP
#define QUEUE_POLICY_BASE_IMPL_HPP

#include <iostream>
#include <cerrno>
#include "qmanager/policies/base/queue_policy_base.hpp"

namespace Flux {
namespace queue_manager {

int queue_policy_base_t::insert (std::shared_ptr<job_t> job)
{
    return detail::queue_policy_base_impl_t::insert (job);
}

int queue_policy_base_t::remove (flux_jobid_t id)
{
    return detail::queue_policy_base_impl_t::remove (id);
}

const std::shared_ptr<job_t> queue_policy_base_t::lookup (flux_jobid_t id)
{
    return detail::queue_policy_base_impl_t::lookup (id);
}

int queue_policy_base_t::reconstruct (std::shared_ptr<job_t> running_job)
{
    return detail::queue_policy_base_impl_t::reconstruct (running_job);
}

std::shared_ptr<job_t> queue_policy_base_t::pending_pop ()
{
    return detail::queue_policy_base_impl_t::pending_pop ();
}

std::shared_ptr<job_t> queue_policy_base_t::alloced_pop ()
{
    return detail::queue_policy_base_impl_t::alloced_pop ();
}

std::shared_ptr<job_t> queue_policy_base_t::complete_pop ()
{
    return detail::queue_policy_base_impl_t::complete_pop ();
}

namespace detail {

int queue_policy_base_impl_t::insert (std::shared_ptr<job_t> job)
{
    int rc = -1;
    if (job == nullptr || m_jobs.find (job->id) != m_jobs.end ()) {
        errno = EINVAL;
        goto out;
    }
    job->state = job_state_kind_t::PENDING;
    job->t_stamps.pending_ts = m_pq_cnt++;
    m_pending.insert (std::pair<uint64_t,
                      flux_jobid_t> (job->t_stamps.pending_ts, job->id));
    m_jobs.insert (std::pair<flux_jobid_t, std::shared_ptr<job_t>> (job->id,
                                                                    job));
    rc = 0;
out:
    return rc;
}

int queue_policy_base_impl_t::remove (flux_jobid_t id)
{
    int rc = -1;
    std::shared_ptr<job_t> job = nullptr;

    if (m_jobs.find (id) == m_jobs.end ()) {
        errno = EINVAL;
        goto out;
    }

    job = m_jobs[id];
    switch (job->state) {
    case job_state_kind_t::PENDING:
        m_pending.erase (job->t_stamps.pending_ts);
        job->state = job_state_kind_t::CANCELED;
        m_jobs.erase (id);
        break;
    case job_state_kind_t::ALLOC_RUNNING:
        m_alloced.erase (job->t_stamps.running_ts);
        // deliberately fall through
    case job_state_kind_t::RUNNING:
        m_running.erase (job->t_stamps.running_ts);
        job->t_stamps.complete_ts = m_cq_cnt++;
        job->state = job_state_kind_t::COMPLETE;
        m_complete.insert (std::pair<uint64_t, flux_jobid_t> (
                               job->t_stamps.complete_ts, job->id));
        break;
    default:
        break;
    }
    rc = 0;
out:
    return rc;
}

const std::shared_ptr<job_t> queue_policy_base_impl_t::lookup (flux_jobid_t id)
{
    std::shared_ptr<job_t> job = nullptr;
    if (m_jobs.find (id) == m_jobs.end ()) {
        errno = ENOENT;
        return job;
    }
    return m_jobs[id];
}

int queue_policy_base_impl_t::reconstruct (std::shared_ptr<job_t> job)
{
    int rc = -1;
    if (job == nullptr || m_jobs.find (job->id) != m_jobs.end ()) {
        errno = EINVAL;
        goto out;
    }
    job->t_stamps.running_ts = m_rq_cnt++;
    m_running.insert (std::pair<uint64_t, flux_jobid_t>(job->t_stamps.running_ts,
                                                        job->id));
    m_jobs.insert (std::pair<flux_jobid_t, std::shared_ptr<job_t>> (job->id,
                                                                    job));
    rc = 0;
out:
    return rc;
}

std::map<uint64_t, flux_jobid_t>::iterator queue_policy_base_impl_t::
    to_running (std::map<uint64_t, flux_jobid_t>::iterator pending_iter,
                bool use_alloced_queue)
{
    flux_jobid_t id = pending_iter->second;
    if (m_jobs.find (id) == m_jobs.end ()) {
        errno = EINVAL;
        return pending_iter;
    }

    std::shared_ptr<job_t> job = m_jobs[id];
    job->state = job_state_kind_t::RUNNING;
    job->t_stamps.running_ts = m_rq_cnt++;
    m_running.insert (std::pair<uint64_t, flux_jobid_t>(
                          job->t_stamps.running_ts, job->id));
    if (use_alloced_queue) {
        job->state = job_state_kind_t::ALLOC_RUNNING;
        m_alloced.insert (std::pair<uint64_t, flux_jobid_t>(
                              job->t_stamps.running_ts, job->id));
    }
    // Return the next iterator after pending_iter. This way,
    // the upper layer can modify m_pending while iterating the queue
    return m_pending.erase (pending_iter);
}

std::map<uint64_t, flux_jobid_t>::iterator queue_policy_base_impl_t::
    to_complete (std::map<uint64_t, flux_jobid_t>::iterator running_iter)
{
    flux_jobid_t id = running_iter->second;
    if (m_jobs.find (id) == m_jobs.end ()) {
        errno = EINVAL;
        return running_iter;
    }

    std::shared_ptr<job_t> job = m_jobs[id];
    job->state = job_state_kind_t::COMPLETE;
    job->t_stamps.complete_ts = m_cq_cnt++;
    m_complete.insert (std::pair<uint64_t, flux_jobid_t>(
                           job->t_stamps.complete_ts, job->id));
    m_alloced.erase (job->t_stamps.running_ts);
    return m_running.erase (running_iter);
}

std::shared_ptr<job_t> queue_policy_base_impl_t::pending_pop ()
{
    std::shared_ptr<job_t> job;
    flux_jobid_t id;

    if (m_pending.empty ())
        return nullptr; 
    id = m_pending.begin ()->second;
    if (m_jobs.find (id) == m_jobs.end ())
        return nullptr;
    job = m_jobs[id];
    m_pending.erase (job->t_stamps.pending_ts);
    m_jobs.erase (id);
    return job;
}

std::shared_ptr<job_t> queue_policy_base_impl_t::alloced_pop ()
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

std::shared_ptr<job_t> queue_policy_base_impl_t::complete_pop ()
{
    std::shared_ptr<job_t> job;
    flux_jobid_t id;
    if (m_complete.empty ())
        return nullptr;
    id = m_complete.begin ()->second;
    if (m_jobs.find (id) == m_jobs.end ())
        return nullptr;
    job = m_jobs[id];
    m_complete.erase (job->t_stamps.complete_ts);
    m_jobs.erase (id);
    return job;
}

} // namespace Flux::queue_manager::detail
} // namespace Flux::queue_manager
} // namespace Flux

#endif // QUEUE_POLICY_BASE_IMPL_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
