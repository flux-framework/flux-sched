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

#ifndef QUEUE_POLICY_BASE_HPP
#define QUEUE_POLICY_BASE_HPP

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>
}

#include <map>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <cstdint>

#include "qmanager/config/queue_system_defaults.hpp"

namespace Flux {
namespace queue_manager {

enum class job_state_kind_t { INIT,
                              PENDING,
                              REJECTED,
                              RUNNING,
                              ALLOC_RUNNING,
                              CANCELED,
                              COMPLETE };

/*! Type to store schedule information such as the
 *  allocated or reserved (for backfill) resource set (R).
 */
struct schedule_t {
    schedule_t () = default;
    schedule_t (const std::string &r) : R (r) { }
    schedule_t (schedule_t &&s) = default;
    schedule_t (const schedule_t &s) = default;
    schedule_t& operator= (schedule_t &&s) = default;
    schedule_t& operator= (const schedule_t &s) = default;
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
};

/*! Type to store a job's attributes.
 */
class job_t {
public:
    ~job_t () { flux_msg_destroy (msg); }
    job_t () = default;
    job_t (job_state_kind_t s, flux_jobid_t jid,
           uint32_t uid, unsigned int p, double t_s, const std::string &R)
	   : state (s), id (jid), userid (uid),
	     priority (p), t_submit (t_s), schedule (R) { }
    job_t (job_t &&j) = default;
    job_t (const job_t &j) = default;
    job_t& operator= (job_t &&s) = default;
    job_t& operator= (const job_t &s) = default;

    bool is_pending () { return state == job_state_kind_t::PENDING; }

    flux_msg_t *msg = NULL;
    job_state_kind_t state = job_state_kind_t::INIT;
    flux_jobid_t id = 0;
    uint32_t userid = 0;
    unsigned int priority = 0;
    double t_submit = 0.0f;;
    std::string jobspec = "";
    std::string note = "";
    t_stamps_t t_stamps;
    schedule_t schedule;
};


namespace detail {
class queue_policy_base_impl_t
{
public:
    int insert (std::shared_ptr<job_t> job);
    int remove (flux_jobid_t id);
    const std::shared_ptr<job_t> lookup (flux_jobid_t id);
    bool is_schedulable ();
    void set_schedulability (bool scheduable);
    bool is_scheduled ();
    void reset_scheduled ();
    bool is_sched_loop_active ();
    void set_sched_loop_active (bool active);

protected:
    int reconstruct_queue (std::shared_ptr<job_t> running_job);
    int pending_reprioritize (flux_jobid_t id, unsigned int priority);
    std::shared_ptr<job_t> pending_pop ();
    std::shared_ptr<job_t> alloced_pop ();
    std::shared_ptr<job_t> rejected_pop ();
    std::shared_ptr<job_t> complete_pop ();
    std::shared_ptr<job_t> reserved_pop ();
    std::map<std::vector<double>, flux_jobid_t>::iterator to_running (
        std::map<std::vector<double>,
                 flux_jobid_t>::iterator pending_iter,
        bool use_alloced_queue);
    std::map<uint64_t, flux_jobid_t>::iterator to_complete (
        std::map<uint64_t, flux_jobid_t>::iterator running_iter);
    std::map<std::vector<double>, flux_jobid_t>::iterator to_rejected (
        std::map<std::vector<double>,
                 flux_jobid_t>::iterator pending_iter,
        const std::string &note);

    bool m_schedulable = false;
    bool m_scheduled = false;
    bool m_sched_loop_active = false;
    uint64_t m_pq_cnt = 0;
    uint64_t m_rq_cnt = 0;
    uint64_t m_dq_cnt = 0;
    uint64_t m_cq_cnt = 0;
    uint64_t m_oq_cnt = 0;
    unsigned int m_queue_depth = DEFAULT_QUEUE_DEPTH;
    unsigned int m_max_queue_depth = MAX_QUEUE_DEPTH;
    std::map<std::vector<double>, flux_jobid_t> m_pending;
    std::map<uint64_t, flux_jobid_t> m_running;
    std::map<uint64_t, flux_jobid_t> m_alloced;
    std::map<uint64_t, flux_jobid_t> m_complete;
    std::map<uint64_t, flux_jobid_t> m_rejected;
    std::map<flux_jobid_t, std::shared_ptr<job_t>> m_jobs;
    std::unordered_map<std::string, std::string> m_qparams;
    std::unordered_map<std::string, std::string> m_pparams;
};
} // namespace Flux::queue_manager::detail


/*! Queue policy base interface abstract class. Derived classes must
 *  implement its run_sched_loop and destructor methods. Insert, remove
 *  and pending_pop interface implementations are provided through
 *  its parent class (detail::queue_policy_base_impl_t).
 */
class queue_policy_base_t : public detail::queue_policy_base_impl_t
{
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
     *  \return          0 on success; -1 on error.
     *                       EINVAL: invalid argument.
     */
    virtual int run_sched_loop (void *h, bool use_alloced_queue) = 0;

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
    virtual int reconstruct_resource (void *h, std::shared_ptr<job_t> job,
                                      std::string &ret_R) = 0;

    /*! Set queue parameters. Can be called multiple times.
     *
     * \param params     comma-delimited key-value pairs string
     *                   (e.g., "queue-depth=1024,foo=bar")
     * \return           0 on success; -1 on error.
     *                       EINVAL: invalid argument.
     */
    int set_queue_params (const std::string &params);

    /*! Set queue policy parameters. Can be called multiple times.
     *
     * \param params     comma-delimited key-value pairs string
     *                   (e.g., "reservation-depth=10,foo=bar")
     * \return           0 on success; -1 on error.
     *                       EINVAL: invalid argument.
     */
    int set_policy_params (const std::string &params);

    /*! Apply the set policy parameters to the queuing policy.
     */
    virtual int apply_params ();

    /*! Get queue and policy parameters.
     *
     * \param q_p        string to which to print queue parameters
     *                   (e.g., "queue-depth=1024,foo=bar")
     * \param p_p        string to which to print queue parameters
     *                   (e.g., "reservation-depth=1024,foo=bar")
     */
    void get_params (std::string &q_p, std::string &p_p);

    /*! Return the queue depth used for this queue. The queue depth
     *  is the depth of its pending-job queue only upto which it
     *  considers for scheduling to deal with unbounded queue length.
     */
    unsigned int get_queue_depth ();

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
    int insert (std::shared_ptr<job_t> pending_job);

    /*! Remove a job whose jobid is id from any internal queues
     *  (e.g., pending queue, running queue, and alloced queue.)
     *  If succeeds, it changes the pending queue or resource
     *  state. This queue becomes "schedulable" if pending job
     *  queue is not empty: i.e., is_schedulable() returns true;
     *
     *  \param id        jobid of flux_jobid_t type.
     *  \return          0 on success; -1 on error.
     *                       ENOENT: unknown id.
     */
    int remove (flux_jobid_t id);

    /*! Look up a job whose jobid is id
     *
     *  \param id        jobid of flux_jobid_t type.
     *  \return          a shared pointer pointing to the job on success;
     *                       nullptr on error. ENOENT: unknown id.
     */
    const std::shared_ptr<job_t> lookup (flux_jobid_t id);

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
    int reconstruct (void *h, std::shared_ptr<job_t> job, std::string &R_out);

    /*! Reprioritize a job with a new priority.
     *
     *  \param id        jobid of flux_jobid_t type.
     *  \param priority  new job priority
     *  \return          0 on success; -1 on error.
     *                       ENOENT: unknown id.
     *                       EEXIST: id already exists
     *                       EINVAL: job not pending
     */
    int pending_reprioritize (flux_jobid_t id, unsigned int priority);

    /*! Pop the first job from the pending job queue. The popped
     *  job is completely graduated from the queue policy layer.
     *
     *  \return          a shared pointer pointing to a job_t object
     *                   on success; nullptr when the queue is empty.
     */
    std::shared_ptr<job_t> pending_pop ();

    /* Query the first job from the pending job queue.
     * \return           a shared pointer pointing to a job_t object
     *                   on success; nullptr when the queue is empty.
     */
    std::shared_ptr<job_t> pending_begin ();

    /* Query the next job from the pending job queue.
     * \return           a shared pointer pointing to a job_t object
     *                   on success; nullptr when the queue is empty.
     */
    std::shared_ptr<job_t> pending_next ();

    /*! Pop the first job from the alloced job queue. The popped
     *  job still remains in the queue policy layer (i.e., in the
     *  internal running job queue).
     *  \return          a shared pointer pointing to a job_t object
     *                   on success; nullptr when the queue is empty.
     */
    std::shared_ptr<job_t> alloced_pop ();

    /*! Pop the first job from the rejected job queue.
     *  The popped is completely graduated from the queue policy layer.
     *  \return          a shared pointer pointing to a job_t object
     *                   on success; nullptr when the queue is empty.
     */
    std::shared_ptr<job_t> rejected_pop ();

    /*! Pop the first job from the internal completed job queue.
     *  The popped is completely graduated from the queue policy layer.
     *  \return          a shared pointer pointing to a job_t object
     *                   on success; nullptr when the queue is empty.
     */
    std::shared_ptr<job_t> complete_pop ();

    /*! Return true if this queue has become schedulable since
     *  its state had been reset with set_schedulability (false).
     *  "Being schedulable" means one or more job or resource events
     *  have occurred such a way that the scheduler should run the
     *  scheduling loop for the pending jobs: e.g., a new job was
     *  inserted into the pending job queue or a job was removed from
     *  the running job queue so that its resource was released.
     */
    bool is_schedulable ();

    /*! Set this queue's schedulability. After this call,
     *  is_schedulable() will return the newly set schedulability
     *  until a new job or resource event occurs.
     */
    void set_schedulability (bool schedulable);

    /*! Return true if the job state of this queue has changed
     *  as the result of the invocation of schedule loop and
     *  and/or of other conditions.
     */
    bool is_scheduled ();

    /*! Reset this queue's "scheduled" state.
     */
    void reset_scheduled ();

    /*! Return true if the asynchronous execution of the scheduling loop
     *  is active.
     */
    bool is_sched_loop_active ();

    /*! Set the asynchronous execution state of the scheduling loop.
     */
    void set_sched_loop_active (bool active);

private:
    int set_params (const std::string &params,
                    std::unordered_map<std::string, std::string> &p_map);
    int set_param (std::string &kv,
                   std::unordered_map<std::string, std::string> &p_map);
    bool is_number (const std::string &num_str);

    std::map<std::vector<double>, flux_jobid_t>::iterator m_pending_iter;
    bool m_iter_valid = false;
};

} // namespace Flux::queue_manager
} // namespace Flux

#endif // QUEUE_POLICY_BASE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
