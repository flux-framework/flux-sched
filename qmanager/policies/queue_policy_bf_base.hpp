/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef QUEUE_POLICY_BF_BASE_HPP
#define QUEUE_POLICY_BF_BASE_HPP

#include "qmanager/policies/base/queue_policy_base.hpp"

namespace Flux {
namespace queue_manager {
namespace detail {

template<class reapi_type>
class queue_policy_bf_base_t : public queue_policy_base_t
{
public:
    virtual ~queue_policy_bf_base_t ();
    virtual int run_sched_loop (void *h, bool use_alloced_queue);
    virtual int reconstruct_resource (void *h, std::shared_ptr<job_t> job,
                                      std::string &R_out);
    virtual int apply_params ();

protected:
    unsigned int m_reservation_depth;
    unsigned int m_max_reservation_depth = MAX_RESERVATION_DEPTH;

private:
    int cancel_completed_jobs (void *h);
    int cancel_reserved_jobs (void *h);
    std::map<std::vector<double>, flux_jobid_t>::iterator &
        allocate_orelse_reserve (void *h, std::shared_ptr<job_t> job,
                                 bool use_alloced_queue,
                                 std::map<std::vector<double>,
                                     flux_jobid_t>::iterator &iter);
    std::map<std::vector<double>, flux_jobid_t>::iterator &
        allocate (void *h, std::shared_ptr<job_t> job, bool use_alloced_queue,
        std::map<std::vector<double>, flux_jobid_t>::iterator &iter);
    int allocate_orelse_reserve_jobs (void *h, bool use_alloced_queue);
    std::map<uint64_t, flux_jobid_t> m_reserved;
    unsigned int m_reservation_cnt;
};

} // namespace Flux::queue_manager::detail
} // namespace Flux::queue_manager
} // namespace Flux

#endif // QUEUE_POLICY_BF_BASE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

