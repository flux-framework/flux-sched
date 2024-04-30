/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
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
#include "src/common/libtap/tap.h"

using namespace Flux;
using namespace Flux::resource_model;

int test_match_policy()
{
    for (auto i = policies.begin(); i != policies.end(); i++) {
        const std::string policy = i->first;
        bool check = known_match_policy(policy);
        if (!check) {
            std::cout << "failed on policy " << policy << std::endl;
            return -1;
        }
    }
    bool check = known_match_policy("asdf");
    if (!check) {
        std::cout << "failed, as it should" << std::endl;
    }
    return 0;
}

int test_policy_settings()
{
    return 0;
}

int main(int argc, char* argv[]) {
    int ret = test_match_policy();
    done_testing();
    return ret;
}

/*
 * vi: ts=4 sw=4 expandtab
 */
