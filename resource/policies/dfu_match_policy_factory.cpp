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
    if (policies.contains (policy)) {
        return true;
    }
    return false;
}

bool parse_bool_match_options (const std::string match_option, const std::string policy_options)
{
    // Return anything from after the = and before space or newline
    size_t spot = policy_options.find (match_option, 0);
    size_t start_pos = policy_options.find ("=", spot);
    size_t end_pos = policy_options.find (" ", spot);
    size_t end_str = policy_options.length ();
    std::string return_opt;
    if ((end_pos == std::string::npos) && (start_pos != std::string::npos)) {
        return_opt = policy_options.substr ((start_pos + 1), (end_str - start_pos - 1));
    } else {
        return_opt = policy_options.substr ((start_pos + 1), (end_pos - start_pos - 1));
    }
    if (return_opt == "true") {
        return true;
    }
    return false;
}

bool option_exists (const std::string match_option, const std::string policy_options)
{
    size_t found = policy_options.find (match_option, 0);
    if (found == std::string::npos) {
        return false;
    }
    return true;
}

int parse_int_match_options (const std::string match_option, const std::string policy_options)
{
    size_t spot = policy_options.find (match_option, 0);
    size_t start_pos = policy_options.find ("=", spot);
    size_t end_pos = policy_options.find (" ", spot);
    size_t end_str = policy_options.length ();
    int return_opt;
    if ((end_pos == std::string::npos) && (start_pos != std::string::npos)) {
        return_opt = stoi (policy_options.substr ((start_pos + 1), (end_str - start_pos - 1)));
    } else {
        return_opt = stoi (policy_options.substr ((start_pos + 1), (end_pos - start_pos - 1)));
    }
    return return_opt;
}

std::shared_ptr<dfu_match_cb_t> create_match_cb (const std::string &policy_requested)
{
    std::string policy = policies.find (policy_requested)->second;
    std::shared_ptr<dfu_match_cb_t> matcher = nullptr;
    try {
        if (policy_requested == "locality") {
            matcher = std::make_shared<greater_interval_first_t> ();
        }
        if (policy_requested == "variation") {
            matcher = std::make_shared<var_aware_t> ();
        }

        if (parse_bool_match_options ("high", policy)) {
            std::shared_ptr<high_first_t> ptr = std::make_shared<high_first_t> ();
            if (parse_bool_match_options ("node_centric", policy)) {
                ptr->add_score_factor (node_rt, 1, 10000);
            }

            if (parse_bool_match_options ("node_exclusive", policy)) {
                ptr->add_exclusive_resource_type (node_rt);
            }

            if (option_exists ("stop_on_k_matches", policy)) {
                ptr->set_stop_on_k_matches (parse_int_match_options ("stop_on_k_matches", policy));
            }
            matcher = ptr;

        } else if (parse_bool_match_options ("low", policy)) {
            std::shared_ptr<low_first_t> ptr = std::make_shared<low_first_t> ();
            if (parse_bool_match_options ("node_centric", policy)) {
                ptr->add_score_factor (node_rt, 1, 10000);
            }

            if (parse_bool_match_options ("node_exclusive", policy)) {
                ptr->add_exclusive_resource_type (node_rt);
            }

            if (option_exists ("stop_on_k_matches", policy)) {
                ptr->set_stop_on_k_matches (parse_int_match_options ("stop_on_k_matches", policy));
            }
            matcher = ptr;
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
