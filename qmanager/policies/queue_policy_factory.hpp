/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef QUEUE_POLICY_FACTORY_HPP
#define QUEUE_POLICY_FACTORY_HPP

#include <memory>
#include <string>
#include "qmanager/policies/base/queue_policy_base.hpp"

namespace Flux {
namespace queue_manager {

std::shared_ptr<queue_policy_base_t> create_queue_policy (const std::string &policy,
                                                          const std::string &reapi);

}  // namespace queue_manager
}  // namespace Flux

#endif  // QUEUE_POLICY_FACTORY_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
