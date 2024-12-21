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
#include <array>
#include <string>
#include <boost/algorithm/string.hpp>
#include "resource/policies/dfu_match_policy_factory.hpp"

namespace Flux {
namespace resource_model {

policies =
    {{"first", "high=true node_centric=true stop_on_1_matches=true"},
     {"firstnodex",
      "high=true node_centric=true node_exclusive=true stop_on_1_matches=true"},
     {"high", "high=true"},
     {"low", "low=true"},
     {"lonode", "low=true node_centric=true"},
     {"hinode", "high=true node_centric=true"},
     {"lonodex", "low=true node_centric=true node_exclusive=true"},
     {"hinodex", "high=true node_centric=true node_exclusive=true"},
     {"locality", ""},
     {"variation", ""}};

const std::vector<std::string> policy_options = {"name", "high", "node_centric", "stop_on_1_matches", "node_exclusive", "low"};

/* This takes a string of match policy options, deliniated by
 * spaces and equals signs, and turns them into a std::map.
 * It validates that each option is set to either "true" or "false"
 * and puts the corresponding boolean in the map.
 */
bool parse_custom_match_policy (const std::string long_string, std::map<std::string, bool> &split) {
    std::vector<std::string> temp;
    boost::split(temp, long_string, boost::is_any_of(" "));
    /* After splitting based on term, validate the terms. If invalid,
     * abort. 
     */
    for (int i=0; i < temp.size(); i++) {
        std::vector<std::string> temp_two;
        /* This split should take an entire string and split it
         * on the equals sign. If there's no equals sign, maybe it will
         * get caught later?
         */
        boost::split(temp_two, temp.at(i), boost::is_any_of("="));
        /* Place the terms in a map for ease of access. Lots of gross
         * temporary structures needed here, hopefully they'll get 
         * garbage collected soon.
         */
        if (std::find(policy_options.begin(), policy_options.end(), temp_two.at(0)) == policy_options.end()) {
            // better way to log something?
            std::cout << "invalid policy option: " << temp_two.at(0) << std::endl;
            return false;
        } else {
            if (temp_two.at(1).compare("true") == 0) {
                const auto [it, success] = split.insert({temp_two.at(0), true});
                if (success == false) {
                    std::cout << "Insertion of " << it->first << " failed" << std::endl;
                }
            } else if (temp_two.at(1).compare("false") == 0) {
                const auto [it, success] = split.insert({temp_two.at(0), false});
                if (success == false) {
                    std::cout << "Insertion of " << it->first << " failed" << std::endl;
                    return false;
                } 
            } else {
                std::cout << "Policy option " << temp_two.at(0) << " requires true or false, got " << temp_two.at(1) << std::endl;
                return false;
            }
        }
    }
    return true;
}

bool known_match_policy (const std::string &policy)
{
    std::map<std::string, bool> throw_away;
    if (policies.contains (policy)) {
        return true;
    } else if (parse_custom_match_policy (policy, throw_away) == true) {
        return true;
    }
    return false;
}

bool parse_bool_match_options (const std::string match_option, std::map<std::string, bool> &policy)
{
    if ((policy.find(match_option) != policy.end()) && (policy[match_option] == true)) {
        return true;
    }
    return false;
}

std::shared_ptr<dfu_match_cb_t> create_match_cb (const std::string &policy_requested)
{
    std::map<std::string, bool> policy;
    if (policies.contains (policy_requested)) {
        parse_custom_match_policy(policies.find (policy_requested)->second, policy);
    } else {
        parse_custom_match_policy (policy_requested, policy);
    }
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

            if (parse_bool_match_options("stop_on_1_matches", policy)) {
                ptr->set_stop_on_k_matches (1);
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

            if (parse_bool_match_options("stop_on_1_matches", policy)) {
                ptr->set_stop_on_k_matches (1);
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
