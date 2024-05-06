/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef QUEUE_POLICY_CONSERVATIVE_HPP
#define QUEUE_POLICY_CONSERVATIVE_HPP

#include "qmanager/policies/queue_policy_bf_base.hpp"

namespace Flux {
namespace queue_manager {
namespace detail {

template<class reapi_type>
class queue_policy_conservative_t : public queue_policy_bf_base_t<reapi_type>
{
public:
    virtual ~queue_policy_conservative_t ();
    queue_policy_conservative_t ();
    queue_policy_conservative_t (
        const queue_policy_conservative_t &p) = default;
    queue_policy_conservative_t (queue_policy_conservative_t &&p) = default;
    queue_policy_conservative_t &operator= (
        const queue_policy_conservative_t &p) = default;
    queue_policy_conservative_t &operator= (
        queue_policy_conservative_t &&p) = default;

    virtual int apply_params ();
};

} // namespace Flux::queue_manager::detail
} // namespace Flux::queue_manager
} // namespace Flux

#endif // QUEUE_POLICY_CONSERVATIVE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
