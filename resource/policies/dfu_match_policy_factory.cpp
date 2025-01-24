/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

extern "C" {
#if HAVE_CONFIG_H
#include <config.h>
#endif
}

#include <string>
#include "resource/policies/dfu_match_policy_factory.hpp"

namespace Flux {
namespace resource_model {

bool known_match_policy (const std::string &policy)
{
    bool rc = true;
    if (policy != FIRST_MATCH && policy != FIRST_NODEX_MATCH && policy != HIGH_ID_FIRST
        && policy != LOW_ID_FIRST && policy != LOW_NODE_FIRST && policy != HIGH_NODE_FIRST
        && policy != LOW_NODEX_FIRST && policy != HIGH_NODEX_FIRST && policy != LOCALITY_AWARE
        && policy != VAR_AWARE && policy != COSCHED_AWARE)
        rc = false;

    return rc;
}

std::shared_ptr<dfu_match_cb_t> create_match_cb (const std::string &policy)
{
    std::shared_ptr<dfu_match_cb_t> matcher = nullptr;

    resource_type_t node_rt ("node");
    try {
        if (policy == FIRST_MATCH || policy == FIRST_NODEX_MATCH) {
            std::shared_ptr<high_first_t> ptr = std::make_shared<high_first_t> ();
            ptr->add_score_factor (node_rt, 1, 10000);
            ptr->set_stop_on_k_matches (1);
            if (policy == FIRST_NODEX_MATCH)
                ptr->add_exclusive_resource_type (node_rt);
            matcher = ptr;
        } else if (policy == HIGH_ID_FIRST) {
            matcher = std::make_shared<high_first_t> ();
        } else if (policy == LOW_ID_FIRST) {
            matcher = std::make_shared<low_first_t> ();
        } else if (policy == LOW_NODE_FIRST || policy == LOW_NODEX_FIRST) {
            std::shared_ptr<low_first_t> ptr = std::make_shared<low_first_t> ();
            ptr->add_score_factor (node_rt, 1, 10000);
            if (policy == LOW_NODEX_FIRST)
                ptr->add_exclusive_resource_type (node_rt);
            matcher = ptr;
        } else if (policy == HIGH_NODE_FIRST || policy == HIGH_NODEX_FIRST) {
            std::shared_ptr<high_first_t> ptr = std::make_shared<high_first_t> ();
            ptr->add_score_factor (node_rt, 1, 10000);
            if (policy == HIGH_NODEX_FIRST)
                ptr->add_exclusive_resource_type (node_rt);
            matcher = ptr;
        } else if (policy == LOCALITY_AWARE) {
            matcher = std::make_shared<greater_interval_first_t> ();
        } else if (policy == VAR_AWARE) {
            matcher = std::make_shared<var_aware_t> ();
        }else if(policy ==  COSCHED_AWARE){
            matcher = std::make_shared<cosched_aware_t> ();

        }
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        matcher = nullptr;
    }

    return matcher;
}

}  // namespace resource_model
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
