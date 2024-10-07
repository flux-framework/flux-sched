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

bool known_match_policy (const std::string &policy);

const std::map<std::string, std::string> policies =
    {{"first", "name=FIRST_MATCH high=true node_centric=true stop_on_k_matches=1"},
     {"firstnodex",
      "name=FIRST_NODEX_MATCH high=true node_centric=true node_exclusive=true stop_on_k_matches=1"},
     {"high", "name=HIGH_ID_FIRST high=true"},
     {"low", "name=LOW_ID_FIRST low=true"},
     {"lonode", "name=LOW_NODE_FIRST low=true node_centric=true"},
     {"hinode", "name=HIGH_NODE_FIRST high=true node_centric=true"},
     {"lonodex", "name=LOW_NODEX_FIRST low=true node_centric=true node_exclusive=true"},
     {"hinodex", "name=HIGH_NODEX_FIRST high=true node_centric=true node_exclusive=true"},
     {"locality", "name=LOCALITY_AWARE"},
     {"variation", "name=VAR_AWARE"},
     {"custom", ""}};

bool parse_bool_match_options (const std::string match_option, const std::string policy_options);

bool option_exists (const std::string match_option, const std::string policy_options);

int parse_int_match_options (const std::string match_option, const std::string policy_options);

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
