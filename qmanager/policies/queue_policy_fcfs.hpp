/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef QUEUE_POLICY_FCFS_HPP
#define QUEUE_POLICY_FCFS_HPP

#include <jansson.h>
#include "qmanager/policies/base/queue_policy_base.hpp"

namespace Flux {
namespace queue_manager {
namespace detail {

template<class reapi_type>
class queue_policy_fcfs_t : public queue_policy_base_t {
   public:
    int run_sched_loop (void *h, bool use_alloced_queue) override;
    int reconstruct_resource (void *h, std::shared_ptr<job_t> job, std::string &R_out) override;
    int apply_params () override;
    const std::string_view policy () const override
    {
        return "fcfs";
    }
    int handle_match_success (flux_jobid_t jobid,
                              const char *status,
                              const char *R,
                              int64_t at,
                              double ov) override;
    int handle_match_failure (flux_jobid_t jobid, int errcode) override;
    int cancel (void *h,
                flux_jobid_t id,
                const char *R,
                bool noent_ok,
                bool &full_removal) override;
    int cancel (void *h, flux_jobid_t id, bool noent_ok) override;

   private:
    int pack_jobs (json_t *jobs);
    int allocate_jobs (void *h, bool use_alloced_queue);
    bool m_queue_depth_limit = false;
    job_map_iter m_iter;
};

}  // namespace detail
}  // namespace queue_manager
}  // namespace Flux

#endif  // QUEUE_POLICY_FCFS_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
