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
#include "resource/policies/base/dfu_match_cb.hpp"
#include "resource/policies/dfu_match_high_id_first.hpp"
#include "resource/policies/dfu_match_low_id_first.hpp"
#include "resource/policies/dfu_match_locality.hpp"
#include "resource/policies/dfu_match_var_aware.hpp"
#include "resource/policies/dfu_match_multilevel_id.hpp"
#include "resource/policies/dfu_match_multilevel_id_impl.hpp"
#include "resource/policies/dfu_match_cosched_aware.hpp"

namespace Flux {
namespace resource_model {

const std::string FIRST_MATCH = "first";
const std::string FIRST_NODEX_MATCH = "firstnodex";
const std::string HIGH_ID_FIRST = "high";
const std::string LOW_ID_FIRST = "low";
const std::string LOW_NODE_FIRST = "lonode";
const std::string HIGH_NODE_FIRST = "hinode";
const std::string LOW_NODEX_FIRST = "lonodex";
const std::string HIGH_NODEX_FIRST = "hinodex";
const std::string LOCALITY_AWARE = "locality";
const std::string VAR_AWARE = "variation";
const std::string COSCHED_AWARE = "cosched";

bool known_match_policy (const std::string &policy);

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
