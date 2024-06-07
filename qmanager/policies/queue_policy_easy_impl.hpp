/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef QUEUE_POLICY_EASY_IMPL_HPP
#define QUEUE_POLICY_EASY_IMPL_HPP

#include "qmanager/policies/queue_policy_bf_base_impl.hpp"

namespace Flux {
namespace queue_manager {
namespace detail {

template<class reapi_type>
queue_policy_easy_t<reapi_type>::~queue_policy_easy_t ()
{
}

template<class reapi_type>
int queue_policy_easy_t<reapi_type>::apply_params ()
{
    return queue_policy_base_t::apply_params ();
}

template<class reapi_type>
queue_policy_easy_t<reapi_type>::queue_policy_easy_t ()
{
    queue_policy_bf_base_t<reapi_type>::m_reservation_depth = 1;
}

}  // namespace detail
}  // namespace queue_manager
}  // namespace Flux

#endif  // QUEUE_POLICY_EASY_IMPL_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
