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
#include "config.h"
#endif
}

#include <cerrno>
#include "planner.h"
#include "src/common/libtap/tap.h"

static int test_planner_avail_resources_at_errno ()
{
    planner_t *ctx = planner_new (0, 100, 10, "core");
    ok (ctx != nullptr, "planner_new succeeded");

    // Test with NULL ctx
    errno = 0;
    int64_t result = planner_avail_resources_at (nullptr, 50);
    ok (result == -1 && errno == EINVAL,
        "planner_avail_resources_at (NULL ctx) returns -1 with errno=EINVAL");

    // Test with at beyond plan_end
    errno = 0;
    result = planner_avail_resources_at (ctx, 101);
    ok (result == -1 && errno == ERANGE,
        "planner_avail_resources_at returns -1 with errno=ERANGE when at > plan_end");

    // Test with at before plan_start
    errno = 0;
    result = planner_avail_resources_at (ctx, -1);
    ok (result == -1 && errno == ERANGE,
        "planner_avail_resources_at returns -1 with errno=ERANGE when at < plan_start");

    planner_destroy (&ctx);
    return 0;
}

static int test_planner_avail_resources_during_errno ()
{
    planner_t *ctx = planner_new (0, 100, 10, "core");
    ok (ctx != nullptr, "planner_new succeeded");

    // Test with NULL ctx
    errno = 0;
    int64_t result = planner_avail_resources_during (nullptr, 50, 10);
    ok (result == -1 && errno == EINVAL,
        "planner_avail_resources_during (NULL ctx) returns -1 with errno=EINVAL");

    // Test with at+duration > plan_end
    errno = 0;
    result = planner_avail_resources_during (ctx, 95, 10);
    ok (result == -1 && errno == ERANGE,
        "planner_avail_resources_during returns -1 with errno=ERANGE when at+duration > plan_end");

    planner_destroy (&ctx);
    return 0;
}

static int test_planner_span_functions_errno ()
{
    planner_t *ctx = planner_new (0, 100, 10, "core");
    ok (ctx != nullptr, "planner_new succeeded");

    // Test planner_span_start_time with invalid span ID
    errno = 0;
    int64_t result = planner_span_start_time (ctx, 999);
    ok (result == -1 && errno == EINVAL,
        "planner_span_start_time (invalid span_id) returns -1 with errno=EINVAL");

    // Test planner_span_start_time with NULL ctx
    errno = 0;
    result = planner_span_start_time (nullptr, 1);
    ok (result == -1 && errno == EINVAL,
        "planner_span_start_time (NULL ctx) returns -1 with errno=EINVAL");

    // Test planner_span_duration with NULL ctx
    errno = 0;
    result = planner_span_duration (nullptr, 1);
    ok (result == -1 && errno == EINVAL,
        "planner_span_duration (NULL ctx) returns -1 with errno=EINVAL");

    // Test planner_span_resource_count with NULL ctx
    errno = 0;
    result = planner_span_resource_count (nullptr, 1);
    ok (result == -1 && errno == EINVAL,
        "planner_span_resource_count (NULL ctx) returns -1 with errno=EINVAL");

    planner_destroy (&ctx);
    return 0;
}

static int test_planner_rem_span_errno ()
{
    planner_t *ctx = planner_new (0, 100, 10, "core");
    ok (ctx != nullptr, "planner_new succeeded");

    // Test planner_rem_span with invalid span ID
    errno = 0;
    int rc = planner_rem_span (ctx, 999);
    ok (rc == -1 && errno == EINVAL,
        "planner_rem_span (invalid span_id) returns -1 with errno=EINVAL");

    // Test planner_rem_span with NULL ctx
    errno = 0;
    rc = planner_rem_span (nullptr, 1);
    ok (rc == -1 && errno == EINVAL, "planner_rem_span (NULL ctx) returns -1 with errno=EINVAL");

    planner_destroy (&ctx);
    return 0;
}

static int test_planner_avail_during_ebusy ()
{
    // Create planner with 10 resources available from time 0 to 100
    planner_t *ctx = planner_new (0, 100, 10, "core");
    ok (ctx != nullptr, "planner_new succeeded");

    // Allocate 8 resources from time 10 to 20
    int64_t span_id = planner_add_span (ctx, 10, 10, 8);
    ok (span_id >= 0, "planner_add_span succeeded");

    // Try to allocate 5 resources during the same time - should fail with EBUSY
    // (only 2 resources available, need 5)
    errno = 0;
    int rc = planner_avail_during (ctx, 10, 10, 5);
    ok (rc == -1 && errno == EBUSY,
        "planner_avail_during returns -1 with errno=EBUSY when resources unavailable");

    // Verify resources are available before and after the span
    errno = 0;
    rc = planner_avail_during (ctx, 0, 10, 10);
    ok (rc == 0, "planner_avail_during succeeds before allocated span");

    errno = 0;
    rc = planner_avail_during (ctx, 20, 10, 10);
    ok (rc == 0, "planner_avail_during succeeds after allocated span");

    planner_destroy (&ctx);
    return 0;
}

static int test_planner_avail_during_erange ()
{
    // Create planner with time range 0 to 100
    planner_t *ctx = planner_new (0, 100, 10, "core");
    ok (ctx != nullptr, "planner_new succeeded");

    // Try to check availability with at+duration > plan_end (95 + 10 = 105 > 100)
    errno = 0;
    int rc = planner_avail_during (ctx, 95, 10, 5);
    ok (rc == -1 && errno == ERANGE,
        "planner_avail_during returns -1 with errno=ERANGE when at+duration > plan_end");

    // Verify it works when at+duration == plan_end
    errno = 0;
    rc = planner_avail_during (ctx, 90, 10, 5);
    ok (rc == 0, "planner_avail_during succeeds when at+duration == plan_end");

    planner_destroy (&ctx);
    return 0;
}

int main (int argc, char *argv[])
{
    plan (NO_PLAN);

    test_planner_avail_resources_at_errno ();
    test_planner_avail_resources_during_errno ();
    test_planner_span_functions_errno ();
    test_planner_rem_span_errno ();
    test_planner_avail_during_ebusy ();
    test_planner_avail_during_erange ();

    done_testing ();
    return EXIT_SUCCESS;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
