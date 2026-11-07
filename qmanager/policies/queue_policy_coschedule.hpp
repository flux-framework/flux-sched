/*****************************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef QUEUE_POLICY_COSCHEDULE_HPP
#define QUEUE_POLICY_COSCHEDULE_HPP

#include "qmanager/policies/queue_policy_bf_base.hpp"

namespace Flux {
namespace queue_manager {
namespace detail {

/* Backfill that keeps a held job in place.
 *
 * A held job is one submitted with attributes.system.hold. Under this policy
 * it is reserved every scheduling loop instead of allocated, so it keeps its
 * footprint while work packs around it, and it is allocated from the front
 * once the sched-fluxion-qmanager.release RPC clears the hold. That is what
 * a job needs when it waits on something the scheduler does not manage, such
 * as a session on a remote device that another job arranges on its behalf.
 *
 * Jobs without the attribute are scheduled exactly as backfill schedules
 * them. No other policy acts on the hold.
 */
template<class reapi_type>
class queue_policy_coschedule_t : public queue_policy_bf_base_t<reapi_type> {
   public:
    virtual ~queue_policy_coschedule_t ();
    queue_policy_coschedule_t ();
    queue_policy_coschedule_t (const queue_policy_coschedule_t &p) = default;
    queue_policy_coschedule_t (queue_policy_coschedule_t &&p) = default;
    queue_policy_coschedule_t &operator= (const queue_policy_coschedule_t &p) = default;
    queue_policy_coschedule_t &operator= (queue_policy_coschedule_t &&p) = default;
    int apply_params () override;
    const std::string_view policy () const override
    {
        return "coschedule";
    }

   protected:
    int next_match_iter () override;
};

}  // namespace detail
}  // namespace queue_manager
}  // namespace Flux

#endif  // QUEUE_POLICY_COSCHEDULE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
