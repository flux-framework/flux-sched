/*****************************************************************************\
 * Copyright 2024 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

/*
 * Test for the dfu_match_policy class(es). Compares shared pointers
 * expected values to string inputs to these classes. Strings can be
 * specified in config for custom policies, or some are provided.
 */

extern "C" {
#if HAVE_CONFIG_H
#include <config.h>
#endif
}

#include <string>
#include <boost/lexical_cast.hpp>
#include "resource/policies/dfu_match_policy_factory.hpp"
#include "src/common/libtap/tap.h"

using namespace Flux;
using namespace Flux::resource_model;

int test_parsers ()
{
    std::map<std::string, bool> container;
    bool first = Flux::resource_model::parse_custom_match_policy("high=true node_centric=true node_exclusive=true stop_on_1_matches=false blah=4", container);
    ok (first == false, "blah is an unrecognized policy option");

    std::map<std::string, bool> container2;
    bool second = Flux::resource_model::parse_custom_match_policy("high=1 node_centric=true node_exclusive=true stop_on_1_matches=false", container2);
    ok (second == false, "1 is an invalid option, must be true or false");

    std::map<std::string, bool> container3;
    bool third = Flux::resource_model::parse_custom_match_policy("high=true node_centric=true stop_on_1_matches=true", container3);
    ok (third == true, "first is a valid policy");

    // little debugging helper for printing the container
    // std::map<std::string, bool>::iterator it;
    // for (it=container.begin(); it != container.end(); it++) {
    //     std::cout << it->first << ": " << it->second << std::endl;
    // }
    bool fourth = Flux::resource_model::parse_bool_match_options ("high", container3);
    ok (fourth == true, "policy first uses option high");

    bool fifth = Flux::resource_model::parse_bool_match_options ("node_centric", container3);
    ok (fifth == true, "policy first uses option node_centric");
    
    bool sixth = Flux::resource_model::parse_bool_match_options ("stop_on_1_matches", container3);
    ok (sixth == true, "policy first uses option stop_on_1_matches");

    bool seventh = Flux::resource_model::parse_bool_match_options ("low", container3);
    ok (seventh == false, "policy first does not use option low");
    
    return 0;
}

int main (int argc, char *argv[])
{
    plan (7);
    test_parsers ();
    done_testing ();
    return 0;
}

/*
 * vi: ts=4 sw=4 expandtab
 */
