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
#include "resource/policies/dfu_match_policy_factory.hpp"
#include "src/common/libtap/tap.h"

using namespace Flux;
using namespace Flux::resource_model;

int test_parsers ()
{
    std::string policy_opts = policies.find ("first")->second;
    bool first = Flux::resource_model::parse_bool_match_options ("high", policy_opts);
    ok (first == true, "policy first uses option high");

    policy_opts = policies.find ("high")->second;
    bool second = Flux::resource_model::option_exists ("stop_on_k_matches", policy_opts);
    ok (second == false, "policy high does not have stop_on_k_matches as substring");

    policy_opts = policies.find ("firstnodex")->second;
    bool third = Flux::resource_model::parse_bool_match_options ("node_exclusive", policy_opts);
    ok (third == true, "policy first uses option node_centric");

    bool fourth = Flux::resource_model::option_exists ("stop_on_k_matches", policy_opts);
    ok (fourth == true, "policy firstnodex has stop_on_k_matches as substring");

    int fifth = Flux::resource_model::parse_int_match_options ("stop_on_k_matches", policy_opts);
    ok (fifth == 1, "policy firstnodex stops on first match");

    return 0;
}

int main (int argc, char *argv[])
{
    plan (5);
    test_parsers ();
    done_testing ();
    return 0;
}

/*
 * vi: ts=4 sw=4 expandtab
 */
