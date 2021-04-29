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

#ifndef QUEUE_POLICY_FCFS_IMPL_HPP
#define QUEUE_POLICY_FCFS_IMPL_HPP

#include "qmanager/policies/queue_policy_fcfs.hpp"
#include "qmanager/policies/base/queue_policy_base_impl.hpp"

namespace Flux {
namespace queue_manager {
namespace detail {


/******************************************************************************
 *                                                                            *
 *                    Private Methods of Queue Policy FCFS                    *
 *                                                                            *
 ******************************************************************************/

template<class reapi_type>
int queue_policy_fcfs_t<reapi_type>::cancel_completed_jobs (void *h)
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
int queue_policy_fcfs_t<reapi_type>::pack_jobs (json_t *jobs)
{
    unsigned int qd = 0;
    std::shared_ptr<job_t> job;
    auto iter = m_pending.begin ();
    while (iter != m_pending.end () && qd < m_queue_depth) {
        json_t *jobdesc;
        job = m_jobs[iter->second];
        if ( !(jobdesc = json_pack ("{s:I s:s}",
                                      "jobid", job->id,
                                      "jobspec", job->jobspec.c_str ()))) {
            json_decref (jobs);
            errno = ENOMEM;
            return -1;
        }
        if (json_array_append_new (jobs, jobdesc) < 0) {
            json_decref (jobs);
            errno = ENOMEM;
            return -1;
        }
        iter++;
        qd++;
    }
    return 0;
}

template<class reapi_type>
int queue_policy_fcfs_t<reapi_type>::allocate_jobs (void *h,
                                                    bool use_alloced_queue)
{
    json_t *jobs = nullptr;
    char *jobs_str = nullptr;
    std::map<std::vector<double>, flux_jobid_t>::iterator iter;

    // move jobs in m_pending_provisional queue into
    // m_pending. Note that c++11 doesn't have a clean way
    // to "move" elements between two std::map objects so
    // we use copy for the time being.
    m_pending.insert (m_pending_provisional.begin (),
                      m_pending_provisional.end ());
    m_pending_provisional.clear ();
    m_iter = m_pending.begin ();
    if (m_pending.empty ())
        return 0;
    if (!(jobs = json_array ())) {
        errno = ENOMEM;
        return -1;
    }
    if (pack_jobs (jobs) < 0)
        return -1;

    set_sched_loop_active (true);
    if ( !(jobs_str = json_dumps (jobs, JSON_INDENT (0)))) {
        errno = ENOMEM;
        json_decref (jobs);
        return -1;
     }
    json_decref (jobs);
    if (reapi_type::match_allocate_multi (h, false, jobs_str, this) < 0) {
        free (jobs_str);
        set_sched_loop_active (false);
        return -1;;
    };
    free (jobs_str);
    return 0;
}

template<class reapi_type>
int queue_policy_fcfs_t<reapi_type>::handle_match_success (
                                         int64_t jobid, const char *status,
                                         const char *R, int64_t at, double ov)
{
    if (!is_sched_loop_active ()) {
        errno = EINVAL;
        return -1;
    }
    std::shared_ptr<job_t> job = m_jobs[m_iter->second];
    if (job->id != static_cast<flux_jobid_t> (jobid)) {
        errno = EINVAL;
        return -1;
    }
    job->schedule.reserved = std::string ("RESERVED") == status?  true : false;
    job->schedule.R = R;
    job->schedule.at = at;
    job->schedule.ov = ov;
    m_iter = to_running (m_iter, true);
    return 0;
}

template<class reapi_type>
int queue_policy_fcfs_t<reapi_type>::handle_match_failure (int errcode)
{
    if (!is_sched_loop_active ()) {
        errno = EINVAL;
        return -1;
    }
    if (errcode != EBUSY && errcode != ENODATA) {
        m_iter = to_rejected (m_iter,
                              (errcode == ENODEV)? "unsatisfiable"
                                                 : "match error");
    }
    return 0;
}

template<class reapi_type>
bool queue_policy_fcfs_t<reapi_type>::get_sloop_active ()
{
    return is_sched_loop_active ();
}

template<class reapi_type>
void queue_policy_fcfs_t<reapi_type>::set_sloop_active (bool active)
{
    set_sched_loop_active (active);
}


/******************************************************************************
 *                                                                            *
 *                    Public API of Queue Policy FCFS                         *
 *                                                                            *
 ******************************************************************************/

template<class reapi_type>
queue_policy_fcfs_t<reapi_type>::~queue_policy_fcfs_t ()
{

}

template<class reapi_type>
int queue_policy_fcfs_t<reapi_type>::apply_params ()
{
    return queue_policy_base_t::apply_params ();
}

template<class reapi_type>
int queue_policy_fcfs_t<reapi_type>::run_sched_loop (void *h,
                                                     bool use_alloced_queue)
{
    if (is_sched_loop_active ())
        return 1;
    int rc = 0;
    set_schedulability (false);
    rc = cancel_completed_jobs (h);
    rc += allocate_jobs (h, use_alloced_queue);
    return rc;
}

template<class reapi_type>
int queue_policy_fcfs_t<reapi_type>::reconstruct_resource (
        void *h, std::shared_ptr<job_t> job, std::string &R_out)
{
    return reapi_type::update_allocate (h, job->id, job->schedule.R,
                                        job->schedule.at,
                                        job->schedule.ov, R_out);
}


} // namespace Flux::queue_manager::detail
} // namespace Flux::queue_manager
} // namespace Flux

#endif // QUEUE_POLICY_FCFS_IMPL_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
