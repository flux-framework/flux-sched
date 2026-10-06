/*****************************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef QUEUE_POLICY_COSCHEDULE_IMPL_HPP
#define QUEUE_POLICY_COSCHEDULE_IMPL_HPP

#include "resource/policies/base/match_op.h"

#include "qmanager/policies/queue_policy_coschedule.hpp"
#include "qmanager/policies/queue_policy_bf_base_impl.hpp"

namespace Flux {
namespace queue_manager {
namespace detail {

template<class reapi_type>
queue_policy_coschedule_t<reapi_type>::~queue_policy_coschedule_t ()
{
}

template<class reapi_type>
int queue_policy_coschedule_t<reapi_type>::apply_params ()
{
    return queue_policy_base_t::apply_params ();
}

template<class reapi_type>
queue_policy_coschedule_t<reapi_type>::queue_policy_coschedule_t ()
{
    // one reservation, as easy has. A held job is reserved on every loop
    // and a deeper window would only block more space for longer.
    queue_policy_bf_base_t<reapi_type>::m_reservation_depth = 1;
}

/* Reserve a held job instead of matching it. The reservation is cancelled
 * at the top of the next loop with the other reservations and made again,
 * so it blocks the space for one loop at a time. When the hold is cleared
 * the job is matched as any other and allocates from the front.
 */
template<class reapi_type>
int queue_policy_coschedule_t<reapi_type>::next_match_iter ()
{
    using bf = queue_policy_bf_base_t<reapi_type>;

    if (bf::m_in_progress_iter == queue_policy_base_t::m_pending.end ())
        return bf::next_match_iter ();

    auto job_it = queue_policy_base_t::m_jobs.find (bf::m_in_progress_iter->second);
    if (job_it == queue_policy_base_t::m_jobs.end () || !job_it->second->hold)
        return bf::next_match_iter ();

    json_t *spec = nullptr;
    json_t *arr = nullptr;
    json_error_t err;
    int rc;

    if (!(spec = json_loads (job_it->second->jobspec.c_str (), 0, &err))) {
        errno = ENOMEM;
        return -1;
    }
    if (!(arr = json_pack ("[{s:I s:o}]",
                           "jobid",
                           static_cast<json_int_t> (job_it->second->id),
                           "jobspec",
                           spec))) {
        json_decref (spec);
        errno = ENOMEM;
        return -1;
    }
    rc = reapi_type::match_allocate_multi (bf::m_handle, match_op_t::MATCH_RESERVE, arr, this);
    json_decref (arr);
    return rc;
}

}  // namespace detail
}  // namespace queue_manager
}  // namespace Flux

#endif  // QUEUE_POLICY_COSCHEDULE_IMPL_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
