/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef QUEUE_POLICY_HYBRID_IMPL_HPP
#define QUEUE_POLICY_HYBRID_IMPL_HPP

#include "qmanager/policies/queue_policy_hybrid.hpp"

namespace Flux {
namespace queue_manager {
namespace detail {

template<class reapi_type>
queue_policy_hybrid_t<reapi_type>::~queue_policy_hybrid_t ()
{
}

template<class reapi_type>
queue_policy_hybrid_t<reapi_type>::queue_policy_hybrid_t ()
{
    queue_policy_bf_base_t<reapi_type>::m_reservation_depth = HYBRID_RESERVATION_DEPTH;
}

template<class reapi_type>
int queue_policy_hybrid_t<reapi_type>::apply_params ()
{
    int rc = queue_policy_base_t::apply_params ();
    int depth = 0;
    try {
        std::unordered_map<std::string, std::string>::const_iterator i;
        if ((i = queue_policy_base_t ::m_pparams.find ("max-reservation-depth"))
            != queue_policy_base_t::m_pparams.end ()) {
            if ((depth = std::stoi (i->second)) < 1) {
                errno = ERANGE;
                rc += -1;
            }
            queue_policy_bf_base_t<reapi_type>::m_max_reservation_depth = depth;
        }
        if ((i = queue_policy_base_t ::m_pparams.find ("reservation-depth"))
            != queue_policy_base_t::m_pparams.end ()) {
            if ((depth = std::stoi (i->second)) < 1) {
                errno = ERANGE;
                rc += -1;
            }
            if (static_cast<unsigned> (depth)
                < queue_policy_bf_base_t<reapi_type>::m_max_reservation_depth) {
                queue_policy_bf_base_t<reapi_type>::m_reservation_depth = depth;
            } else {
                queue_policy_bf_base_t<reapi_type>::m_reservation_depth =
                    queue_policy_bf_base_t<reapi_type>::m_reservation_depth;
            }
        }
        rc += 0;
    } catch (const std::invalid_argument &e) {
        errno = EINVAL;
    } catch (const std::out_of_range &e) {
        errno = ERANGE;
    }
    return rc;
}

}  // namespace detail
}  // namespace queue_manager
}  // namespace Flux

#endif  // QUEUE_POLICY_HYBRID_IMPL_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
