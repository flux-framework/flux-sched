/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef QUEUE_POLICY_FACTORY_IMPL_HPP
#define QUEUE_POLICY_FACTORY_IMPL_HPP

#include "resource/reapi/bindings/c++/reapi.hpp"
#include "resource/reapi/bindings/c++/reapi_module.hpp"
#include "resource/reapi/bindings/c++/reapi_module_impl.hpp"
#include "qmanager/policies/base/queue_policy_base.hpp"
#include "qmanager/policies/queue_policy_fcfs.hpp"
#include "qmanager/policies/queue_policy_fcfs_impl.hpp"
#include "qmanager/policies/queue_policy_easy.hpp"
#include "qmanager/policies/queue_policy_easy_impl.hpp"
#include "qmanager/policies/queue_policy_hybrid.hpp"
#include "qmanager/policies/queue_policy_hybrid_impl.hpp"
#include "qmanager/policies/queue_policy_conservative.hpp"
#include "qmanager/policies/queue_policy_conservative_impl.hpp"
#include <string>

namespace Flux {
namespace queue_manager {
namespace detail {

using namespace resource_model;
using namespace resource_model::detail;

std::shared_ptr<queue_policy_base_t> create_queue_policy (const std::string &policy,
                                                          const std::string &reapi)
{
    std::shared_ptr<queue_policy_base_t> p = nullptr;

    try {
        if (policy == "fcfs") {
            if (reapi == "module")
                p = std::make_shared<queue_policy_fcfs_t<reapi_module_t>> ();
        } else if (policy == "easy") {
            if (reapi == "module")
                p = std::make_shared<queue_policy_easy_t<reapi_module_t>> ();
        } else if (policy == "hybrid") {
            if (reapi == "module")
                p = std::make_shared<queue_policy_hybrid_t<reapi_module_t>> ();
        } else if (policy == "conservative") {
            if (reapi == "module")
                p = std::make_shared<queue_policy_conservative_t<reapi_module_t>> ();
        }
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        p = nullptr;
    }

    return p;
}

}  // namespace detail
}  // namespace queue_manager
}  // namespace Flux

#endif  // QUEUE_POLICY_FACTORY_IMPL_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
