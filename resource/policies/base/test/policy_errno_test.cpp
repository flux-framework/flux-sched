/*****************************************************************************\
 * Copyright 2024 Lawrence Livermore National Security, LLC
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

#include <cerrno>
#include "resource/policies/dfu_match_high_id_first.hpp"
#include "resource/policies/dfu_match_multilevel_id_impl.hpp"
#include "src/common/libtap/tap.h"

using namespace Flux::resource_model;

static int test_set_stop_on_k_matches_errno ()
{
    high_first_t policy;

    // Test with invalid input (k > 1)
    errno = 0;
    int rc = policy.set_stop_on_k_matches (2);
    ok (rc == -1, "set_stop_on_k_matches returns -1 for k > 1");
    ok (errno == EINVAL, "set_stop_on_k_matches sets errno=EINVAL for k > 1");

    // Test with valid input
    errno = 0;
    rc = policy.set_stop_on_k_matches (1);
    ok (rc == 0, "set_stop_on_k_matches returns 0 for k = 1");
    ok (errno == 0, "set_stop_on_k_matches preserves errno on success");

    return 0;
}

int main (int argc, char *argv[])
{
    plan (NO_PLAN);

    test_set_stop_on_k_matches_errno ();

    done_testing ();
    return EXIT_SUCCESS;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
