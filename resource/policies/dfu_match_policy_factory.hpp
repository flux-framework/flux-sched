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

const std::map<std::string, std::string> policies = {{"first", "FIRST_MATCH"},
                                                     {"firstnodex", "FIRST_NODEX_MATCH"},
                                                     {"high", "HIGH_ID_FIRST"},
                                                     {"low", "LOW_ID_FIRST"},
                                                     {"lonode", "LOW_NODE_FIRST"},
                                                     {"hinode", "HIGH_NODE_FIRST"},
                                                     {"lonodex", "LOW_NODEX_FIRST"},
                                                     {"hinodex", "HIGH_NODEX_FIRST"},
                                                     {"locality", "LOCALITY_AWARE"},
                                                     {"variation", "VAR_AWARE"}};



bool known_match_policy (const std::string &policy);

/*! Factory method for creating a matching callback
 *  object, representing a matching policy.
 */
std::shared_ptr<dfu_match_cb_t> create_match_cb (const std::string &policy);

} // resource_model
} // Flux

#endif // DFU_MATCH_POLICY_FACTORY_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
