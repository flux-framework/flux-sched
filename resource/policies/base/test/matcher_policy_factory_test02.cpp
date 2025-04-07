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
#include <iostream>
#include <string>
#include <boost/lexical_cast.hpp>
#include "resource/policies/dfu_match_policy_factory.hpp"
#include "src/common/libtap/tap.h"

using namespace Flux;
using namespace Flux::resource_model;

int test_parsers ()
{
    std::map<std::string, bool> container;
    std::string e = "";
    bool first = Flux::resource_model::parse_custom_match_policy (
        "policy=high node_centric=true node_exclusive=true stop_on_1_matches=false blah=4",
        container,
        e);
    ok (first == false, "blah is an unrecognized policy option");
    ok (e == "invalid policy option: blah\n", "correct error message for invalid option");

    std::map<std::string, bool> container2;
    std::string e2 = "";
    bool second = Flux::resource_model::parse_custom_match_policy (
        "policy=1 node_centric=true node_exclusive=true stop_on_1_matches=false", container2, e2);
    ok (second == false, "1 is an invalid option, must be high or low");
    ok (e2 == "policy key within custom policy accepts only low or high\n",
        "correct error message for high/low");

    std::map<std::string, bool> container3;
    std::string e3 = "";
    bool third = Flux::resource_model::
        parse_custom_match_policy ("policy=high node_centric=true stop_on_1_matches=true",
                                   container3,
                                   e3);
    ok (third == true, "first is a valid policy");

    bool fourth = Flux::resource_model::parse_bool_match_options ("high", container3);
    ok (fourth == true, "policy first uses option high");

    bool fifth = Flux::resource_model::parse_bool_match_options ("node_centric", container3);
    ok (fifth == true, "policy first uses option node_centric");

    bool sixth = Flux::resource_model::parse_bool_match_options ("stop_on_1_matches", container3);
    ok (sixth == true, "policy first uses option stop_on_1_matches");

    bool seventh = Flux::resource_model::parse_bool_match_options ("low", container3);
    ok (seventh == false, "policy first does not use option low");

    std::map<std::string, bool> container4;
    std::string e4 = "";
    bool eighth = Flux::resource_model::
        parse_custom_match_policy ("policy=high node_centric=true node_exclusive=4",
                                   container4,
                                   e4);
    ok (eighth == false, "node_exclusive accepts only true or false");
    ok (e4 == "Policy option node_exclusive requires true or false, got 4\n",
        "correct error msg for options");

    return 0;
}

int main (int argc, char *argv[])
{
    plan (11);
    test_parsers ();
    done_testing ();
    return 0;
}

/*
 * vi: ts=4 sw=4 expandtab
 */
