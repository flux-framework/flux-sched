/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef DFU_MATCH_POLICY_FACTORY_HPP
#define DFU_MATCH_POLICY_FACTORY_HPP

#include <string>
#include <memory>
#include <map>
#include "resource/policies/base/dfu_match_cb.hpp"
#include "resource/policies/dfu_match_high_id_first.hpp"
#include "resource/policies/dfu_match_low_id_first.hpp"
#include "resource/policies/dfu_match_locality.hpp"
#include "resource/policies/dfu_match_var_aware.hpp"
#include "resource/policies/dfu_match_multilevel_id.hpp"
#include "resource/policies/dfu_match_multilevel_id_impl.hpp"

namespace Flux {
namespace resource_model {

bool known_match_policy (const std::string &policy, std::string &error);

/* Returns true if the custom match policy is valid.
 * Returns false if it is invalid.
 * Stores all of the options requested in the map under
 * key-value pairs.
 */
bool parse_custom_match_policy (const std::string long_string,
                                std::map<std::string, bool> &split,
                                std::string &error);

/* Included in the header for unit testing.
 * This is how the map keys get accessed and returned easily.
 */
bool parse_bool_match_options (const std::string match_option, std::map<std::string, bool> &policy);

/*! Factory method for creating a matching callback
 *  object, representing a matching policy.
 */
std::shared_ptr<dfu_match_cb_t> create_match_cb (const std::string &policy);

}  // namespace resource_model
}  // namespace Flux

#endif  // DFU_MATCH_POLICY_FACTORY_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
