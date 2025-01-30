/*****************************************************************************\
 * Copyright 2025 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef DFU_TRAVERSER_POLICY_FACTORY_HPP
#define DFU_TRAVERSER_POLICY_FACTORY_HPP

#include <string>
#include <memory>
#include "resource/traversers/dfu.hpp"
#include "resource/traversers/dfu_flexible.hpp"

namespace Flux {
namespace resource_model {

const std::string SIMPLE = "simple";
const std::string FLEXIBLE = "flexible";

bool known_traverser_policy (const std::string &policy);

/*! Factory method for creating a matching callback
 *  object, representing a matching policy.
 */
std::shared_ptr<dfu_traverser_t> create_traverser (const std::string &policy);

}  // namespace resource_model
}  // namespace Flux

#endif  // DFU_TRAVERSER_POLICY_FACTORY_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
