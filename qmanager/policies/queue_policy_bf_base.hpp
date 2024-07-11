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
#include <flux/core/job.h>

namespace Flux {
namespace queue_manager {
namespace detail {

template<class reapi_type>
class queue_policy_bf_base_t : public queue_policy_base_t {
   public:
    virtual ~queue_policy_bf_base_t ();
    virtual int run_sched_loop (void *h, bool use_alloced_queue);
    int cancel_sched_loop () override;
    virtual int reconstruct_resource (void *h, std::shared_ptr<job_t> job, std::string &R_out);
    virtual int apply_params ();
    virtual int handle_match_success (flux_jobid_t jobid,
                                      const char *status,
                                      const char *R,
                                      int64_t at,
                                      double ov);
    virtual int handle_match_failure (flux_jobid_t jobid, int errcode);
    int cancel (void *h,
                flux_jobid_t id,
                const char *R,
                bool noent_ok,
                bool &full_removal) override;

   protected:
    unsigned int m_reservation_depth;
    unsigned int m_max_reservation_depth = MAX_RESERVATION_DEPTH;

   private:
    int next_match_iter ();
    int cancel_reserved_jobs (void *h);
    int allocate_orelse_reserve_jobs (void *h);
    std::map<uint64_t, flux_jobid_t> m_reserved;
    int m_reservation_cnt;
    int m_scheduled_cnt;
    bool m_try_reserve = false;
    decltype (m_pending)::iterator m_in_progress_iter = m_pending.end ();
    void *m_handle = NULL;
};

}  // namespace detail
}  // namespace queue_manager
}  // namespace Flux

#endif  // QUEUE_POLICY_BF_BASE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
