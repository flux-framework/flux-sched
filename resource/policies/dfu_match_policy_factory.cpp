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
#include <boost/algorithm/string.hpp>
#include "resource/policies/dfu_match_policy_factory.hpp"

namespace Flux {
namespace resource_model {

const std::map<std::string, std::string> policies =
    {{"first", "policy=high node_centric=true stop_on_1_matches=true"},
     {"firstnodex", "policy=high node_centric=true node_exclusive=true stop_on_1_matches=true"},
     {"high", "policy=high"},
     {"low", "policy=low"},
     {"lonode", "policy=low node_centric=true"},
     {"hinode", "policy=high node_centric=true"},
     {"lonodex", "policy=low node_centric=true node_exclusive=true"},
     {"hinodex", "policy=high node_centric=true node_exclusive=true"},
     {"locality", ""},
     {"variation", ""}};

const std::vector<std::string> policy_options = {"policy",
                                                 "node_centric",
                                                 "stop_on_1_matches",
                                                 "node_exclusive"};

/* This takes a string of match policy options, deliniated by
 * spaces and equals signs, and turns them into a std::map.
 * It validates that each option is set to either "true" or "false"
 * and puts the corresponding boolean in the map.
 */
bool parse_custom_match_policy (const std::string long_string,
                                std::map<std::string, bool> &split,
                                std::string &error_str)
{
    std::vector<std::string> options;
    boost::split (options, long_string, boost::is_any_of (" "));
    /* After splitting based on term, validate the terms. If invalid,
     * abort.
     */
    for (int i = 0; i < options.size (); i++) {
        std::vector<std::string> settings;
        /* This split should take an entire string and split it
         * on the equals sign. If there's no equals sign, maybe it will
         * get caught later?
         */
        boost::split (settings, options.at (i), boost::is_any_of ("="));
        /* Place the terms in a map for ease of access. Lots of gross
         * temporary structures needed here, hopefully they'll get
         * garbage collected soon.
         */
        if (std::find (policy_options.begin (), policy_options.end (), settings.at (0))
            == policy_options.end ()) {
            error_str += std::basic_string ("invalid policy option: ") += settings.at (0) +=
                std::basic_string ("\n");
            return false;
        } else {
            if (settings.at (0).compare ("policy") == 0) {
                /* This is the one option we want to allow to be something other than "true"
                 * or "false." It should take "low" or "high." Maybe other things in the
                 * future, too.
                 */
                if (settings.at (1).compare ("high") == 0) {
                    const auto [it, success] = split.insert ({"high", true});
                    if (success == false) {
                        error_str += std::basic_string ("Insertion of ") +=
                            std::basic_string (it->first) += std::basic_string (" failed\n");
                    }
                } else if (settings.at (1).compare ("low") == 0) {
                    const auto [it, success] = split.insert ({"low", true});
                    if (success == false) {
                        error_str += std::basic_string ("Insertion of ") +=
                            std::basic_string (it->first) += std::basic_string (" failed\n");
                    }
                } else {
                    error_str += "policy key within custom policy accepts only low or high\n";
                    return false;
                }
            } else if (settings.at (1).compare ("true") == 0) {
                const auto [it, success] = split.insert ({settings.at (0), true});
                if (success == false) {
                    error_str += std::basic_string ("Insertion of ") +=
                        std::basic_string (it->first) += std::basic_string (" failed\n");
                }
            } else if (settings.at (1).compare ("false") == 0) {
                const auto [it, success] = split.insert ({settings.at (0), false});
                if (success == false) {
                    error_str += std::basic_string ("Insertion of ") +=
                        std::basic_string (it->first) += std::basic_string (" failed\n");
                    return false;
                }
            } else {
                error_str += std::basic_string ("Policy option ") +=
                    std::basic_string (settings.at (0)) +=
                    std::basic_string (" requires true or false, got ") +=
                    std::basic_string (settings.at (1)) += std::basic_string ("\n");
                return false;
            }
        }
    }
    return true;
}

bool known_match_policy (const std::string &policy, std::string &error_str)
{
    std::map<std::string, bool> throw_away;
    if (policies.contains (policy)) {
        return true;
    } else if (parse_custom_match_policy (policy, throw_away, error_str) == true) {
        return true;
    }
    return false;
}

bool parse_bool_match_options (const std::string match_option, std::map<std::string, bool> &policy)
{
    if ((policy.find (match_option) != policy.end ()) && (policy[match_option] == true)) {
        return true;
    }
    return false;
}

std::shared_ptr<dfu_match_cb_t> create_match_cb (const std::string &policy_requested)
{
    std::map<std::string, bool> policy;
    std::string error_str;
    if (policies.contains (policy_requested)) {
        parse_custom_match_policy (policies.find (policy_requested)->second, policy, error_str);
    } else {
        parse_custom_match_policy (policy_requested, policy, error_str);
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

            if (parse_bool_match_options ("stop_on_1_matches", policy)) {
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

            if (parse_bool_match_options ("stop_on_1_matches", policy)) {
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
