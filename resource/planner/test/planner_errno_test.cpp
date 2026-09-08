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
#include "planner_multi.h"
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

static int test_planner_null_arg_guards ()
{
    planner_t *ctx = planner_new (0, 100, 10, "core");
    ok (ctx != nullptr, "NULL arg guards: planner_new");

    errno = 0;
    ok (planner_copy (nullptr) == nullptr && errno == EINVAL, "planner_copy: NULL ctx");

    errno = 0;
    ok (planner_assign (nullptr, ctx) == -1 && errno == EINVAL, "planner_assign: NULL lhs");

    errno = 0;
    ok (planner_assign (ctx, nullptr) == -1 && errno == EINVAL, "planner_assign: NULL rhs");

    errno = 0;
    ok (planner_add_span (nullptr, 0, 10, 5) == -1 && errno == EINVAL,
        "planner_add_span: NULL ctx");

    errno = 0;
    ok (planner_update_total (nullptr, 10) == -1 && errno == EINVAL,
        "planner_update_total: NULL ctx");

    // Two NULL planners must compare equal (operator== reflexivity).
    ok (planners_equal (nullptr, nullptr), "planners_equal (NULL, NULL) returns true");
    ok (!planners_equal (ctx, nullptr), "planners_equal (ctx, NULL) returns false");
    ok (!planners_equal (nullptr, ctx), "planners_equal (NULL, ctx) returns false");
    ok (planners_equal (ctx, ctx), "planners_equal (ctx, ctx) returns true");

    planner_destroy (&ctx);
    return 0;
}

static int test_planner_multi_null_arg_guards ()
{
    const uint64_t totals[] = {10, 20};
    const char *types[] = {"core", "memory"};
    planner_multi_t *ctx = planner_multi_new (0, 100, totals, types, 2);
    ok (ctx != nullptr, "NULL arg guards: planner_multi_new");

    errno = 0;
    ok (planner_multi_copy (nullptr) == nullptr && errno == EINVAL, "planner_multi_copy: NULL ctx");

    errno = 0;
    ok (planner_multi_assign (nullptr, ctx) == -1 && errno == EINVAL,
        "planner_multi_assign: NULL lhs");

    errno = 0;
    ok (planner_multi_assign (ctx, nullptr) == -1 && errno == EINVAL,
        "planner_multi_assign: NULL rhs");

    ok (planner_multis_equal (nullptr, nullptr), "planner_multis_equal (NULL, NULL) returns true");
    ok (!planner_multis_equal (ctx, nullptr), "planner_multis_equal (ctx, NULL) returns false");
    ok (!planner_multis_equal (nullptr, ctx), "planner_multis_equal (NULL, ctx) returns false");
    ok (planner_multis_equal (ctx, ctx), "planner_multis_equal (ctx, ctx) returns true");

    errno = 0;
    ok (planner_multi_span_first (nullptr) == -1 && errno == EINVAL,
        "planner_multi_span_first: NULL ctx");

    errno = 0;
    ok (planner_multi_resource_type_at (ctx, 2) == nullptr && errno == EINVAL,
        "planner_multi_resource_type_at: out-of-range index");

    errno = 0;
    ok (planner_multi_resource_total_at (nullptr, 0) == -1 && errno == EINVAL,
        "planner_multi_resource_total_at: NULL ctx");

    errno = 0;
    ok (planner_multi_resource_total_by_type (nullptr, "core") == -1 && errno == EINVAL,
        "planner_multi_resource_total_by_type: NULL ctx");

    errno = 0;
    ok (planner_multi_resource_total_by_type (ctx, nullptr) == -1 && errno == EINVAL,
        "planner_multi_resource_total_by_type: NULL type");

    const uint64_t requests[] = {1, 1};
    errno = 0;
    ok (planner_multi_add_span (nullptr, 0, 10, requests, 2) == -1 && errno == EINVAL,
        "planner_multi_add_span: NULL ctx");

    errno = 0;
    ok (planner_multi_add_span (ctx, 0, 10, nullptr, 2) == -1 && errno == EINVAL,
        "planner_multi_add_span: NULL requests");

    errno = 0;
    ok (planner_multi_add_span (ctx, 0, 10, requests, 1) == -1 && errno == EINVAL,
        "planner_multi_add_span: mismatched len");

    // A NULL element of resource_types[] would be turned into a std::string.
    const char *null_types[] = {"core", nullptr};
    errno = 0;
    ok (planner_multi_new (0, 100, totals, null_types, 2) == nullptr && errno == EINVAL,
        "planner_multi_new: NULL resource type");

    errno = 0;
    ok (planner_multi_update (ctx, totals, null_types, 2) == -1 && errno == EINVAL,
        "planner_multi_update: NULL resource type");

    int64_t span_id = planner_multi_add_span (ctx, 0, 10, requests, 2);
    ok (span_id >= 0, "NULL arg guards: planner_multi_add_span");

    const uint64_t reduced[] = {1, 1};
    bool removed = false;
    errno = 0;
    ok (planner_multi_reduce_span (ctx, span_id, reduced, null_types, 2, removed) == -1
            && errno == EINVAL,
        "planner_multi_reduce_span: NULL resource type");

    planner_multi_destroy (&ctx);
    return 0;
}

static int test_planner_multi_empty_guards ()
{
    planner_multi_t *ctx = planner_multi_empty ();
    ok (ctx != nullptr, "planner_multi_empty succeeded");

    // These would throw std::out_of_range across the C boundary without
    // the empty-planner guards
    errno = 0;
    ok (planner_multi_base_time (ctx) == -1 && errno == EINVAL,
        "planner_multi_base_time: empty planner");

    errno = 0;
    ok (planner_multi_duration (ctx) == -1 && errno == EINVAL,
        "planner_multi_duration: empty planner");

    errno = 0;
    ok (planner_multi_avail_time_next (ctx) == -1 && errno == EINVAL,
        "planner_multi_avail_time_next: empty planner");

    // planner_multi_update derives base_time and duration from the planner
    // at index 0, which an empty planner_multi does not have
    const uint64_t totals[] = {10};
    const char *types[] = {"core"};
    errno = 0;
    ok (planner_multi_update (ctx, totals, types, 1) == -1 && errno == EINVAL,
        "planner_multi_update: empty planner");

    planner_multi_destroy (&ctx);
    return 0;
}

static int test_planner_multi_rem_span_after_planner_delete ()
{
    const uint64_t totals[] = {10, 20};
    const char *types[] = {"core", "memory"};
    planner_multi_t *ctx = planner_multi_new (0, 100, totals, types, 2);
    ok (ctx != nullptr, "rem_span after planner delete: planner_multi_new");

    const uint64_t requests[] = {5, 10};
    int64_t span_id = planner_multi_add_span (ctx, 0, 10, requests, 2);
    ok (span_id >= 0, "rem_span after delete: planner_multi_add_span");

    // Shrink the planner_multi to one resource type; the existing span's
    // lookup vector is now longer than the planner count
    const uint64_t new_totals[] = {10};
    const char *new_types[] = {"core"};
    ok (planner_multi_update (ctx, new_totals, new_types, 1) == 0,
        "planner_multi_update to remove a resource type succeeded");

    // Without the guard this indexes past the planner set and throws.
    errno = 0;
    ok (planner_multi_rem_span (ctx, span_id) == -1 && errno == EINVAL,
        "planner_multi_rem_span: span vector longer than planner set");

    planner_multi_destroy (&ctx);
    return 0;
}

static int test_planner_multi_avail_time_next_after_front_insert ()
{
    const uint64_t totals[] = {10};
    const char *types[] = {"core"};
    planner_multi_t *ctx = planner_multi_new (0, 100, totals, types, 1);
    ok (ctx != nullptr, "avail_time_next after front insertion: planner_multi_new");

    const uint64_t span_requests[] = {5};
    int64_t span_id = planner_multi_add_span (ctx, 10, 10, span_requests, 1);
    ok (span_id >= 0, "front insert: planner_multi_add_span");

    const uint64_t requests[] = {10};
    ok (planner_multi_avail_time_first (ctx, 0, 5, requests, 1) == 0,
        "front insert: avail_time_first");

    // Insert a type at index 0 mid-iteration: the new leading planner's
    // availability iterator was never initialized.
    const uint64_t new_totals[] = {30, 10};
    const char *new_types[] = {"gpu", "core"};
    ok (planner_multi_update (ctx, new_totals, new_types, 2) == 0,
        "planner_multi_update to insert a resource type at index 0 succeeded");

    // A stale continuation must fail with EINVAL, not crash or return stale
    // data; the caller restarts with avail_time_first.
    errno = 0;
    ok (planner_multi_avail_time_next (ctx) == -1 && errno == EINVAL,
        "avail_time_next: EINVAL after front insertion");

    const uint64_t new_requests[] = {1, 10};
    ok (planner_multi_avail_time_first (ctx, 0, 5, new_requests, 2) == 0,
        "planner_multi_avail_time_first after the composition change succeeds");

    planner_multi_destroy (&ctx);
    return 0;
}

static int test_planner_multi_short_span_vector ()
{
    const uint64_t totals[] = {10};
    const char *types[] = {"core"};
    planner_multi_t *ctx = planner_multi_new (0, 100, totals, types, 1);
    ok (ctx != nullptr, "short span vector: planner_multi_new");

    const uint64_t requests[] = {5};
    int64_t span_id = planner_multi_add_span (ctx, 0, 10, requests, 1);
    ok (span_id >= 0, "short span vector: planner_multi_add_span");

    // Grow the planner_multi by one resource type; the existing span's
    // lookup vector is now shorter than the planner count
    const uint64_t new_totals[] = {10, 20};
    const char *new_types[] = {"core", "memory"};
    ok (planner_multi_update (ctx, new_totals, new_types, 2) == 0,
        "planner_multi_update to add a resource type succeeded");

    // The span holds no allocation of a type added after it, so the planned
    // count is 0 rather than an out_of_range throw.
    errno = 0;
    ok (planner_multi_span_planned_at (ctx, span_id, 1) == 0,
        "span_planned_at: 0 for a type added after the span");

    errno = 0;
    ok (planner_multi_span_planned_at (ctx, span_id, 2) == -1 && errno == EINVAL,
        "planner_multi_span_planned_at: out-of-range index");

    // Reducing a span whose lookup vector is shorter than the planner
    // count must fail up front and leave planner state unchanged
    const uint64_t reduced[] = {2};
    const char *reduced_types[] = {"core"};
    bool removed = true;
    errno = 0;
    ok (planner_multi_reduce_span (ctx, span_id, reduced, reduced_types, 1, removed) == -1
            && errno == EINVAL && !removed,
        "planner_multi_reduce_span: short span vector");
    ok (planner_multi_avail_resources_at (ctx, 5, 0) == 5,
        "failed reduce_span: availability unchanged");
    ok (planner_multi_span_planned_at (ctx, span_id, 0) == 5,
        "failed reduce_span: planned count unchanged");

    planner_multi_destroy (&ctx);
    return 0;
}

// planner_span_next must reject every state that leaves the iterator at end ().
static void test_planner_span_iterator_guards ()
{
    planner_t *ctx = planner_new (0, 10, 10, "core");

    int64_t span = planner_add_span (ctx, 0, 5, 3);
    ok (span != -1, "span iterator guards: span added");
    ok (planner_span_first (ctx) == span, "span_first returns the only span");

    errno = 0;
    ok (planner_span_next (ctx) == -1, "span_next past the last span returns -1");
    errno = 0;
    ok (planner_span_next (ctx) == -1,
        "span_next again returns -1 rather than incrementing end ()");

    // A fresh copy has its iterator at end (), so the same guard applies.
    planner_t *copy = planner_copy (ctx);
    ok (copy != nullptr, "planner_copy succeeds");
    errno = 0;
    ok (planner_span_next (copy) == -1, "span_next on a fresh copy returns -1");

    // An empty span map leaves the iterator at end () too.
    planner_t *empty = planner_new (0, 10, 10, "core");
    errno = 0;
    ok (planner_span_next (empty) == -1, "span_next before span_first returns -1");

    planner_destroy (&empty);
    planner_destroy (&copy);
    planner_destroy (&ctx);
}

// The planner_multi_t half of the case above.
static void test_planner_multi_span_iterator_guards ()
{
    const uint64_t totals[] = {10};
    const char *types[] = {"core"};
    const uint64_t requests[] = {3};
    planner_multi_t *ctx = planner_multi_new (0, 10, totals, types, 1);

    int64_t span = planner_multi_add_span (ctx, 0, 5, requests, 1);
    ok (span != -1, "multi span iterator guards: span added");
    ok (planner_multi_span_first (ctx) == span, "multi span_first returns the only span");

    errno = 0;
    ok (planner_multi_span_next (ctx) == -1 && errno == ENOENT,
        "multi span_next past the last span returns -1");
    errno = 0;
    ok (planner_multi_span_next (ctx) == -1 && errno == ENOENT,
        "multi span_next again returns -1 rather than incrementing end ()");

    planner_multi_t *copy = planner_multi_copy (ctx);
    ok (copy != nullptr, "planner_multi_copy succeeds");
    errno = 0;
    ok (planner_multi_span_next (copy) == -1 && errno == ENOENT,
        "multi span_next on a fresh copy returns -1");

    planner_multi_t *empty = planner_multi_new (0, 10, totals, types, 1);
    errno = 0;
    ok (planner_multi_span_first (empty) == -1 && errno == ENOENT,
        "multi span_first on an empty span map returns -1");

    // Nothing has set this planner's iterator; it must start out at end ().
    planner_multi_t *fresh = planner_multi_new (0, 10, totals, types, 1);
    errno = 0;
    ok (planner_multi_span_next (fresh) == -1 && errno == ENOENT,
        "multi span_next before span_first returns -1");

    planner_multi_destroy (&fresh);
    planner_multi_destroy (&empty);
    planner_multi_destroy (&copy);
    planner_multi_destroy (&ctx);
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
    test_planner_null_arg_guards ();
    test_planner_multi_null_arg_guards ();
    test_planner_multi_empty_guards ();
    test_planner_multi_rem_span_after_planner_delete ();
    test_planner_multi_avail_time_next_after_front_insert ();
    test_planner_multi_short_span_vector ();
    test_planner_span_iterator_guards ();
    test_planner_multi_span_iterator_guards ();

    done_testing ();
    return EXIT_SUCCESS;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
