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
// planner_t's definition, needed to copy-construct one at the C++ level.
#include "resource/planner/c++/planner.hpp"
#include "resource/planner/c++/planner_multi.hpp"
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
    ok (ctx != nullptr, "planner_new succeeded");

    errno = 0;
    ok (planner_copy (nullptr) == nullptr && errno == EINVAL,
        "planner_copy (NULL) returns NULL with errno=EINVAL");

    errno = 0;
    ok (planner_assign (nullptr, ctx) == -1 && errno == EINVAL,
        "planner_assign (NULL lhs) returns -1 with errno=EINVAL");

    errno = 0;
    ok (planner_assign (ctx, nullptr) == -1 && errno == EINVAL,
        "planner_assign (NULL rhs) returns -1 with errno=EINVAL");

    errno = 0;
    ok (planner_add_span (nullptr, 0, 10, 5) == -1 && errno == EINVAL,
        "planner_add_span (NULL ctx) returns -1 with errno=EINVAL");

    errno = 0;
    ok (planner_update_total (nullptr, 10) == -1 && errno == EINVAL,
        "planner_update_total (NULL ctx) returns -1 with errno=EINVAL");

    // Null planners must compare equal for operator== reflexivity of
    // vertex data with nullable planner members
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
    ok (ctx != nullptr, "planner_multi_new succeeded");

    errno = 0;
    ok (planner_multi_copy (nullptr) == nullptr && errno == EINVAL,
        "planner_multi_copy (NULL) returns NULL with errno=EINVAL");

    errno = 0;
    ok (planner_multi_assign (nullptr, ctx) == -1 && errno == EINVAL,
        "planner_multi_assign (NULL lhs) returns -1 with errno=EINVAL");

    errno = 0;
    ok (planner_multi_assign (ctx, nullptr) == -1 && errno == EINVAL,
        "planner_multi_assign (NULL rhs) returns -1 with errno=EINVAL");

    ok (planner_multis_equal (nullptr, nullptr), "planner_multis_equal (NULL, NULL) returns true");
    ok (!planner_multis_equal (ctx, nullptr), "planner_multis_equal (ctx, NULL) returns false");
    ok (!planner_multis_equal (nullptr, ctx), "planner_multis_equal (NULL, ctx) returns false");
    ok (planner_multis_equal (ctx, ctx), "planner_multis_equal (ctx, ctx) returns true");

    errno = 0;
    ok (planner_multi_span_first (nullptr) == -1 && errno == EINVAL,
        "planner_multi_span_first (NULL ctx) returns -1 with errno=EINVAL");

    errno = 0;
    ok (planner_multi_resource_type_at (ctx, 2) == nullptr && errno == EINVAL,
        "planner_multi_resource_type_at (out-of-range index) returns NULL with errno=EINVAL");

    const uint64_t requests[] = {1, 1};
    errno = 0;
    ok (planner_multi_add_span (nullptr, 0, 10, requests, 2) == -1 && errno == EINVAL,
        "planner_multi_add_span (NULL ctx) returns -1 with errno=EINVAL");

    errno = 0;
    ok (planner_multi_add_span (ctx, 0, 10, requests, 1) == -1 && errno == EINVAL,
        "planner_multi_add_span (mismatched len) returns -1 with errno=EINVAL");

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
        "planner_multi_base_time (empty planner) returns -1 with errno=EINVAL");

    errno = 0;
    ok (planner_multi_duration (ctx) == -1 && errno == EINVAL,
        "planner_multi_duration (empty planner) returns -1 with errno=EINVAL");

    errno = 0;
    ok (planner_multi_avail_time_next (ctx) == -1 && errno == EINVAL,
        "planner_multi_avail_time_next (empty planner) returns -1 with errno=EINVAL");

    // planner_multi_update derives base_time and duration from the planner
    // at index 0, which an empty planner_multi does not have
    const uint64_t totals[] = {10};
    const char *types[] = {"core"};
    errno = 0;
    ok (planner_multi_update (ctx, totals, types, 1) == -1 && errno == EINVAL,
        "planner_multi_update (empty planner) returns -1 with errno=EINVAL");

    planner_multi_destroy (&ctx);
    return 0;
}

static int test_planner_self_assign ()
{
    planner_t *ctx = planner_new (0, 100, 10, "core");
    ok (ctx != nullptr, "planner_new succeeded");

    int64_t span_id = planner_add_span (ctx, 10, 10, 8);
    ok (span_id >= 0, "planner_add_span succeeded");

    // Self-assignment must be a no-op: without the guard, operator=
    // first erases its own state and then copies from the erased state
    ok (planner_assign (ctx, ctx) == 0, "self-assign returns 0");
    ok (planner_avail_resources_at (ctx, 15) == 2, "self-assign preserves span allocations");
    ok (planner_span_resource_count (ctx, span_id) == 8, "self-assign preserves the span");

    planner_destroy (&ctx);
    return 0;
}

static int test_planner_multi_self_assign ()
{
    const uint64_t totals[] = {10, 20};
    const char *types[] = {"core", "memory"};
    planner_multi_t *ctx = planner_multi_new (0, 100, totals, types, 2);
    ok (ctx != nullptr, "planner_multi_new succeeded");

    const uint64_t requests[] = {5, 10};
    int64_t span_id = planner_multi_add_span (ctx, 0, 10, requests, 2);
    ok (span_id >= 0, "planner_multi_add_span succeeded");

    // Self-assignment must be a no-op: without the guard, operator=
    // first erases its own planners and then copies from the erased state
    ok (planner_multi_assign (ctx, ctx) == 0, "self-assign returns 0");
    ok (planner_multi_resources_len (ctx) == 2, "self-assign preserves the planner set");
    ok (planner_multi_span_planned_at (ctx, span_id, 0) == 5,
        "self-assign preserves span allocations");
    ok (planner_multi_avail_resources_at (ctx, 5, 0) == 5, "self-assign preserves availability");

    planner_multi_destroy (&ctx);
    return 0;
}

static int test_planner_multi_copy_iterator_independent ()
{
    const uint64_t totals[] = {10};
    const char *types[] = {"core"};
    planner_multi_t *orig = planner_multi_new (0, 100, totals, types, 1);
    ok (orig != nullptr, "planner_multi_new succeeded");

    const uint64_t requests[] = {1};
    int64_t s0 = planner_multi_add_span (orig, 0, 10, requests, 1);
    int64_t s1 = planner_multi_add_span (orig, 20, 10, requests, 1);
    ok (s0 >= 0 && s1 >= 0, "added two spans");

    // Position the source's span iterator mid-iteration, then copy
    ok (planner_multi_span_first (orig) == s0, "planner_multi_span_first on the source");
    planner_multi_t *copy = planner_multi_copy (orig);
    ok (copy != nullptr, "planner_multi_copy succeeded");

    // The copy iterates its own span map from the start, independent of
    // the source's iterator position
    ok (planner_multi_span_first (copy) == s0, "planner_multi_span_first on the copy");
    ok (planner_multi_span_next (orig) == s1, "the source's iteration is unaffected by the copy");

    // Destroy the source: the copy's iterator must not reference the
    // source's (now freed) span map
    planner_multi_destroy (&orig);
    ok (planner_multi_span_next (copy) == s1, "the copy's iterator survives destroying the source");
    errno = 0;
    ok (planner_multi_span_next (copy) == -1 && errno == ENOENT,
        "the copy's iteration ends cleanly");

    planner_multi_destroy (&copy);
    return 0;
}

static int test_planner_multi_rem_span_after_planner_delete ()
{
    const uint64_t totals[] = {10, 20};
    const char *types[] = {"core", "memory"};
    planner_multi_t *ctx = planner_multi_new (0, 100, totals, types, 2);
    ok (ctx != nullptr, "planner_multi_new succeeded");

    const uint64_t requests[] = {5, 10};
    int64_t span_id = planner_multi_add_span (ctx, 0, 10, requests, 2);
    ok (span_id >= 0, "planner_multi_add_span succeeded");

    // Shrink the planner_multi to one resource type; the existing span's
    // lookup vector is now longer than the planner count
    const uint64_t new_totals[] = {10};
    const char *new_types[] = {"core"};
    ok (planner_multi_update (ctx, new_totals, new_types, 1) == 0,
        "planner_multi_update to remove a resource type succeeded");

    // Removing the span would index the lookup vector past the end of the
    // planner set and throw std::out_of_range across the C boundary
    // without the guard
    errno = 0;
    ok (planner_multi_rem_span (ctx, span_id) == -1 && errno == EINVAL,
        "planner_multi_rem_span (span vector longer than planner set) returns -1 with "
        "errno=EINVAL");

    planner_multi_destroy (&ctx);
    return 0;
}

static int test_planner_multi_avail_time_next_after_front_insert ()
{
    const uint64_t totals[] = {10};
    const char *types[] = {"core"};
    planner_multi_t *ctx = planner_multi_new (0, 100, totals, types, 1);
    ok (ctx != nullptr, "planner_multi_new succeeded");

    const uint64_t span_requests[] = {5};
    int64_t span_id = planner_multi_add_span (ctx, 10, 10, span_requests, 1);
    ok (span_id >= 0, "planner_multi_add_span succeeded");

    const uint64_t requests[] = {10};
    ok (planner_multi_avail_time_first (ctx, 0, 5, requests, 1) == 0,
        "planner_multi_avail_time_first returns the earliest satisfiable time");

    // Insert a new resource type at index 0 between avail_time_first and
    // avail_time_next: the planner now leading the iteration is one whose
    // availability iterator was never initialized by avail_time_first
    const uint64_t new_totals[] = {30, 10};
    const char *new_types[] = {"gpu", "core"};
    ok (planner_multi_update (ctx, new_totals, new_types, 2) == 0,
        "planner_multi_update to insert a resource type at index 0 succeeded");

    // The contract is that iteration restarts with avail_time_first after
    // a composition change; a stale continuation must fail cleanly with
    // EINVAL (planner_avail_time_next on an uninitialized availability
    // iterator) rather than crash or return stale data
    errno = 0;
    ok (planner_multi_avail_time_next (ctx) == -1 && errno == EINVAL,
        "planner_multi_avail_time_next after front insertion returns -1 with errno=EINVAL");

    // Restarting the iteration works
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
    ok (ctx != nullptr, "planner_multi_new succeeded");

    const uint64_t requests[] = {5};
    int64_t span_id = planner_multi_add_span (ctx, 0, 10, requests, 1);
    ok (span_id >= 0, "planner_multi_add_span succeeded");

    // Grow the planner_multi by one resource type; the existing span's
    // lookup vector is now shorter than the planner count
    const uint64_t new_totals[] = {10, 20};
    const char *new_types[] = {"core", "memory"};
    ok (planner_multi_update (ctx, new_totals, new_types, 2) == 0,
        "planner_multi_update to add a resource type succeeded");

    // Accessing the added resource type index for the pre-existing span
    // would throw std::out_of_range across the C boundary without the
    // span vector bounds guard. The span holds no allocation of the type
    // added after its creation, so the planned count is 0.
    errno = 0;
    ok (planner_multi_span_planned_at (ctx, span_id, 1) == 0,
        "planner_multi_span_planned_at (type added after span creation) returns 0");

    // An out-of-range index must still be an error
    errno = 0;
    ok (planner_multi_span_planned_at (ctx, span_id, 2) == -1 && errno == EINVAL,
        "planner_multi_span_planned_at (out-of-range index) returns -1 with errno=EINVAL");

    // Reducing a span whose lookup vector is shorter than the planner
    // count must fail up front and leave planner state unchanged
    const uint64_t reduced[] = {2};
    const char *reduced_types[] = {"core"};
    bool removed = true;
    errno = 0;
    ok (planner_multi_reduce_span (ctx, span_id, reduced, reduced_types, 1, removed) == -1
            && errno == EINVAL && !removed,
        "planner_multi_reduce_span (short span vector) returns -1 with errno=EINVAL");
    ok (planner_multi_avail_resources_at (ctx, 5, 0) == 5,
        "failed planner_multi_reduce_span leaves availability unchanged");
    ok (planner_multi_span_planned_at (ctx, span_id, 0) == 5,
        "failed planner_multi_reduce_span leaves the span's planned count unchanged");

    planner_multi_destroy (&ctx);
    return 0;
}

static int test_planner_multi_iterator_after_tail_growth ()
{
    const uint64_t totals[] = {10};
    const char *types[] = {"core"};
    planner_multi_t *ctx = planner_multi_new (0, 100, totals, types, 1);
    ok (ctx != nullptr, "planner_multi_new succeeded");

    // Occupy 5 cores in [10, 20) so a request for all 10 cores has a
    // next satisfiable point (t=20) after the first (t=0)
    const uint64_t span_requests[] = {5};
    int64_t span_id = planner_multi_add_span (ctx, 10, 10, span_requests, 1);
    ok (span_id >= 0, "planner_multi_add_span succeeded");

    const uint64_t requests[] = {10};
    ok (planner_multi_avail_time_first (ctx, 0, 5, requests, 1) == 0,
        "planner_multi_avail_time_first returns the earliest satisfiable time");

    // Grow the planner_multi between avail_time_first and avail_time_next;
    // planner_multi_update registers the added type in the iterator request
    // with a zero count
    const uint64_t new_totals[] = {10, 20};
    const char *new_types[] = {"core", "memory"};
    ok (planner_multi_update (ctx, new_totals, new_types, 2) == 0,
        "planner_multi_update to add a resource type succeeded");

    // The iteration must survive the composition change (no exception across
    // the C boundary) and treat the added type as a zero request: the next
    // point at which all 10 cores are free is t=20 (span end)
    errno = 0;
    ok (planner_multi_avail_time_next (ctx) == 20,
        "planner_multi_avail_time_next after planner_multi_update returns the next "
        "satisfiable time");

    planner_multi_destroy (&ctx);
    return 0;
}

// planner_t owns its inner planner and frees it in ~planner_t, so the
// wrapper needs a deep copy constructor. Note that planner_copy () does
// not cover this: it constructs through planner_t (const planner &),
// which takes the inner type and has always deep copied. Only a
// planner_t-from-planner_t construction reaches the constructor that was
// implicitly declared, and shallow, before the rule-of-five change.
static void test_planner_t_value_semantics ()
{
    planner_t *src = planner_new (0, 10, 10, "core");
    int64_t span = planner_add_span (src, 0, 5, 3);

    ok (span != -1, "value semantics: span added to the source planner");
    ok (planner_avail_resources_at (src, 0) == 7, "source reports 7 of 10 available");

    {
        planner_t copy (*src);

        ok (copy.plan != src->plan, "copy construction allocates a distinct inner planner");
        ok (planner_avail_resources_at (&copy, 0) == 7, "the copy sees the source's span");

        // Mutating the source must not reach the copy.
        ok (planner_rem_span (src, span) == 0, "span removed from the source");
        ok (planner_avail_resources_at (src, 0) == 10, "source is back to 10 available");
        ok (planner_avail_resources_at (&copy, 0) == 7, "the copy is unaffected by the source");
    }

    // The copy's destructor ran at scope exit. A shallow copy would have
    // freed the planner src still points at, so everything below this
    // line is a use-after-free under the old implicit copy constructor.
    ok (planner_avail_resources_at (src, 0) == 10, "source survives destruction of the copy");

    planner_t *other = planner_new (0, 10, 4, "core");

    ok (planner_assign (other, src) == 0, "planner_assign succeeds");
    ok (planner_avail_resources_at (other, 0) == 10, "assignment copied the source's state");

    // planner_assign now delegates to the by-value copy-and-swap operator,
    // which is self-assignment safe without an explicit this != &o guard:
    // the parameter is copied before *this is touched.
    ok (planner_assign (src, src) == 0, "self-assignment succeeds");
    ok (planner_avail_resources_at (src, 0) == 10, "self-assignment preserves state");

    planner_destroy (&other);
    planner_destroy (&src);
}

// The planner_multi_t half of the case above. planner_multi_copy () goes
// through planner_multi_t (const planner_multi &), so it does not reach
// the wrapper's own copy constructor either.
static void test_planner_multi_t_value_semantics ()
{
    const uint64_t totals[] = {10, 20};
    const char *types[] = {"core", "gpu"};
    const uint64_t requests[] = {3, 4};

    planner_multi_t *src = planner_multi_new (0, 10, totals, types, 2);
    int64_t span = planner_multi_add_span (src, 0, 5, requests, 2);

    ok (span != -1, "value semantics: span added to the source planner_multi");
    ok (planner_multi_span_planned_at (src, span, 0) == 3, "source records 3 cores");

    {
        planner_multi_t copy (*src);

        ok (copy.plan_multi != src->plan_multi,
            "copy construction allocates a distinct inner planner_multi");
        ok (planner_multi_span_planned_at (&copy, span, 0) == 3, "the copy sees the span");

        // Mutating the source must not reach the copy.
        ok (planner_multi_rem_span (src, span) == 0, "span removed from the source");
        ok (planner_multi_span_size (src) == 0, "source has no spans left");
        ok (planner_multi_span_planned_at (&copy, span, 0) == 3,
            "the copy is unaffected by the source");
    }

    // The copy's destructor ran at scope exit. A shallow copy would have
    // freed the planner_multi src still points at.
    ok (planner_multi_resources_len (src) == 2, "source survives destruction of the copy");

    planner_multi_t *other = planner_multi_new (0, 10, totals, types, 2);

    ok (planner_multi_assign (other, src) == 0, "planner_multi_assign succeeds");
    ok (planner_multis_equal (other, src), "assignment copied the source's state");

    ok (planner_multi_assign (src, src) == 0, "self-assignment succeeds");
    ok (planner_multi_resources_len (src) == 2, "self-assignment preserves state");

    planner_multi_destroy (&other);
    planner_multi_destroy (&src);
}

// planner_span_next increments the span-lookup iterator before testing it
// against end (), so it has to reject the states in which the iterator is
// already there.  The copy case also covers planner's iterator being
// initialized to end () rather than left singular.
static void test_planner_span_iterator_guards ()
{
    planner_t *ctx = planner_new (0, 10, 10, "core");

    errno = 0;
    ok (planner_span_next (ctx) == -1, "span_next before span_first returns -1");

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

    planner_destroy (&copy);
    planner_destroy (&ctx);
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
    test_planner_self_assign ();
    test_planner_multi_self_assign ();
    test_planner_multi_copy_iterator_independent ();
    test_planner_multi_rem_span_after_planner_delete ();
    test_planner_multi_avail_time_next_after_front_insert ();
    test_planner_multi_short_span_vector ();
    test_planner_multi_iterator_after_tail_growth ();

    test_planner_t_value_semantics ();
    test_planner_multi_t_value_semantics ();
    test_planner_span_iterator_guards ();

    done_testing ();
    return EXIT_SUCCESS;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
