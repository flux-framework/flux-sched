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
#include <algorithm>
#include "qmanager/policies/base/queue_policy_base.hpp"

namespace Flux {
namespace queue_manager {

int queue_policy_base_t::set_param (std::string &p_pair,
                                    std::unordered_map<std::string,
                                                       std::string> &p_map)
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
    p_map.insert (std::pair<std::string, std::string>(k, v));
    rc = 0;
done:
    return rc;
}


int queue_policy_base_t::set_params (const std::string &params,
                                     std::unordered_map<std::string,
                                                        std::string> &p_map)
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

int queue_policy_base_t::set_queue_params (const std::string &params)
{
    return set_params (params, m_qparams);
}

int queue_policy_base_t::set_policy_params (const std::string &params)
{
    return set_params (params, m_pparams);
}

int queue_policy_base_t::apply_params ()
{
    int rc = 0;
    int depth = 0;
    try {
        std::unordered_map<std::string, std::string>::const_iterator i;
        if ((i = queue_policy_base_impl_t::m_qparams.find ("max-queue-depth"))
             != queue_policy_base_impl_t::m_qparams.end ()) {
            if ( (depth = std::stoi (i->second)) < 1) {
                errno = ERANGE;
                rc += -1;
            }
            queue_policy_base_impl_t::m_max_queue_depth = depth;
            if (static_cast<unsigned> (depth)
                    < queue_policy_base_impl_t::m_queue_depth) {
                queue_policy_base_impl_t::m_queue_depth = depth;
            }
        }
        if ((i = queue_policy_base_impl_t::m_qparams.find ("queue-depth"))
             != queue_policy_base_impl_t::m_qparams.end ()) {
            int depth = std::stoi (i->second);
            if ( (depth = std::stoi (i->second)) < 1) {
                errno = ERANGE;
                rc += -1;
            }
            if (static_cast<unsigned> (depth) < m_max_queue_depth) {
                queue_policy_base_impl_t::m_queue_depth = depth;
            } else {
                queue_policy_base_impl_t::m_queue_depth
                    = queue_policy_base_impl_t::m_max_queue_depth;
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

void queue_policy_base_t::get_params (std::string &q_p, std::string &p_p)
{
    std::unordered_map<std::string, std::string>::const_iterator i;
    for (i = queue_policy_base_impl_t::m_qparams.begin ();
         i != queue_policy_base_impl_t::m_qparams.end (); i++) {
        if (!q_p.empty ())
            q_p += std::string (",");
        q_p += i->first + std::string ("=") + i->second;
    }
    for (i = queue_policy_base_impl_t::m_pparams.begin ();
         i != queue_policy_base_impl_t::m_pparams.end (); i++) {
        if (!p_p.empty ())
            p_p += std::string (",");
        p_p += i->first + std::string ("=") + i->second;
    }
}

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

int queue_policy_base_t::reconstruct (void *h, std::shared_ptr<job_t> job,
                                      std::string &R_out)
{
    int rc = 0;
    if ( (rc = reconstruct_resource (h, job, R_out)) < 0)
        return rc;
    return detail::queue_policy_base_impl_t::reconstruct_queue (job);
}

std::shared_ptr<job_t> queue_policy_base_t::pending_pop ()
{
    return detail::queue_policy_base_impl_t::pending_pop ();
}

std::shared_ptr<job_t> queue_policy_base_t::pending_begin ()
{
    std::shared_ptr<job_t> job_p = nullptr;
    m_pending_iter = detail::queue_policy_base_impl_t::m_pending.begin ();
    if (m_pending_iter == m_pending.end ()) {
        m_iter_valid = false;
    } else {
        flux_jobid_t id = m_pending_iter->second;
        m_iter_valid = true;
        if (detail::queue_policy_base_impl_t::m_jobs.find (id)
            != detail::queue_policy_base_impl_t::m_jobs.end ())
            job_p = detail::queue_policy_base_impl_t::m_jobs[id];
    }
    return job_p;
}

std::shared_ptr<job_t> queue_policy_base_t::pending_next ()
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
        if (detail::queue_policy_base_impl_t::m_jobs.find (id)
            != detail::queue_policy_base_impl_t::m_jobs.end ())
            job_p = detail::queue_policy_base_impl_t::m_jobs[id];
    }
ret:
    return job_p;
}

std::shared_ptr<job_t> queue_policy_base_t::alloced_pop ()
{
    return detail::queue_policy_base_impl_t::alloced_pop ();
}

std::shared_ptr<job_t> queue_policy_base_t::rejected_pop ()
{
    return detail::queue_policy_base_impl_t::rejected_pop ();
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
    m_pending.insert (std::pair<std::vector<double>, flux_jobid_t> (
        {static_cast<double> (job->priority),
         static_cast<double> (job->t_submit),
         static_cast<double> (job->t_stamps.pending_ts)}, job->id));
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
        m_pending.erase ({static_cast<double> (job->priority),
                          static_cast<double> (job->t_submit),
                          static_cast<double> (job->t_stamps.pending_ts)});
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

int queue_policy_base_impl_t::reconstruct_queue (std::shared_ptr<job_t> job)
{
    int rc = -1;
    std::pair<std::map<uint64_t, flux_jobid_t>::iterator, bool> ret;
    std::pair<std::map<flux_jobid_t,
                       std::shared_ptr<job_t>>::iterator, bool> ret2;

    if (job == nullptr || m_jobs.find (job->id) != m_jobs.end ()) {
        errno = EINVAL;
        goto out;
    }
    job->t_stamps.running_ts = m_rq_cnt++;

    ret = m_running.insert (std::pair<uint64_t, flux_jobid_t>(
                                job->t_stamps.running_ts, job->id));
    if (ret.second == false) {
        rc = -1;
        errno = ENOMEM;
        goto out;
    }
    ret2 = m_jobs.insert (std::pair<flux_jobid_t, std::shared_ptr<job_t>> (
                             job->id, job));
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

std::map<std::vector<double>, flux_jobid_t>::iterator queue_policy_base_impl_t::
    to_running (std::map<std::vector<double>,
                         flux_jobid_t>::iterator pending_iter,
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

std::map<std::vector<double>, flux_jobid_t>::iterator queue_policy_base_impl_t::
    to_rejected (std::map<std::vector<double>,
                          flux_jobid_t>::iterator pending_iter,
                 const std::string &note)
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
    m_rejected.insert (std::pair<uint64_t, flux_jobid_t>(
                          job->t_stamps.rejected_ts, job->id));
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
    m_pending.erase ({static_cast<double> (job->priority),
                      static_cast<double> (job->t_submit),
                      static_cast<double> (job->t_stamps.pending_ts)});
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

std::shared_ptr<job_t> queue_policy_base_impl_t::rejected_pop ()
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
