/*****************************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
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
#include <cstdint>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "resource/planner/c/planner.h"
#include "resource/policies/dfu_match_policy_factory.hpp"
#include "resource/store/resource_graph_store.hpp"
#include "resource/traversers/dfu_impl.hpp"

using namespace Flux::resource_model;
using namespace Flux::resource_model::detail;

namespace {

constexpr int64_t k_base_time = 0;
constexpr uint64_t k_duration = 3600;
constexpr uint64_t k_resource_total = 1;

// dfu_impl_t::remove (vtx_t, int64_t) is driven entirely by
// resource_graph_metadata_t::by_jobid: it looks the jobid up in the index
// and visits exactly those vertices, never touching `root` or graph
// topology otherwise. So a bare two-vertex graph with no edges is
// sufficient scaffolding here -- no reader, no jobspec match, no
// containment subsystem wiring required.
std::shared_ptr<resource_graph_db_t> make_bare_graph (vtx_t &a, vtx_t &b)
{
    auto db = std::make_shared<resource_graph_db_t> ();
    a = boost::add_vertex (db->resource_graph);
    b = boost::add_vertex (db->resource_graph);
    db->resource_graph[a].name = "vtxA";
    db->resource_graph[b].name = "vtxB";
    return db;
}

// Hand-populate vertex v with a full, self-consistent set of job-keyed
// state for jobid -- the tag, exclusive-filter span and schedule
// allocation that upd_txfilter ()/upd_plan () would install during a real
// match -- and index it, mirroring what the traverser does on match
// success.
void allocate_job_on_vertex (resource_graph_db_t &db, vtx_t v, int64_t jobid)
{
    auto &pool = db.resource_graph[v];

    pool.idata.tags[jobid] = jobid;

    pool.idata.x_checker = planner_new (k_base_time, k_duration, k_resource_total, "node");
    REQUIRE (pool.idata.x_checker != nullptr);
    int64_t x_span = planner_add_span (pool.idata.x_checker, k_base_time, k_duration, 1);
    REQUIRE (x_span >= 0);
    pool.idata.x_spans[jobid] = x_span;

    pool.schedule.plans = planner_new (k_base_time, k_duration, k_resource_total, "node");
    REQUIRE (pool.schedule.plans != nullptr);
    int64_t sched_span = planner_add_span (pool.schedule.plans, k_base_time, k_duration, 1);
    REQUIRE (sched_span >= 0);
    pool.schedule.allocations[jobid] = sched_span;

    db.metadata.add_job_vertex (jobid, v);
}

}  // namespace

TEST_CASE ("by_jobid index retains a vertex whose purge fails and a retry visits only it",
           "[by_jobid_index]")
{
    vtx_t a, b;
    auto db = make_bare_graph (a, b);
    auto match = create_match_cb ("first");
    dfu_impl_t impl (db, match);
    // The 2-arg constructor only stashes m_graph_db/m_match; m_graph (the
    // raw pointer cancel_vertex ()/mod_plan () actually dereference) is
    // only set by set_graph_db () -- the same two calls
    // dfu_traverser_t::initialize () makes before using a dfu_impl_t.
    // Skipping this leaves m_graph null and segfaults on first use.
    impl.set_graph_db (db);
    impl.set_match_cb (match);
    const int64_t jobid = 100;

    allocate_job_on_vertex (*db, a, jobid);
    allocate_job_on_vertex (*db, b, jobid);
    REQUIRE (db->metadata.by_jobid.at (jobid).size () == 2);
    REQUIRE (db->metadata.verify_job_index (db->resource_graph));

    // Fault injection: point vertex a's exclusive-filter span at a span id
    // that was never added to its own x_checker planner. Empirically
    // confirmed (see resource/planner/test/planner_errno_test.cpp, which
    // asserts the same on planner_rem_span () with an unknown span id):
    // this makes cancel_vertex () fail deterministically for vertex a
    // alone, without corrupting anything else or crashing the planner.
    //
    // rem_exclusive_filter () erases idata.x_spans[jobid] unconditionally
    // *before* calling planner_rem_span (), so the erase always happens,
    // but planner_rem_span () itself returns -1/EINVAL for the bogus span
    // id. That failure return propagates out of mod_idata () before
    // mod_plan () ever runs, so vertex a's schedule.allocations/
    // schedule.plans state -- and therefore its by_jobid indexing --
    // survives the failed attempt untouched.
    int64_t valid_span = db->resource_graph[a].idata.x_spans.at (jobid);
    int64_t bogus_span = valid_span + 424242;
    db->resource_graph[a].idata.x_spans[jobid] = bogus_span;

    errno = 0;
    REQUIRE (impl.remove (a, jobid) == -1);

    // Vertex b purged cleanly and dropped out of the index; vertex a's
    // purge failed and it alone remains indexed.
    auto job_it = db->metadata.by_jobid.find (jobid);
    REQUIRE (job_it != db->metadata.by_jobid.end ());
    REQUIRE (job_it->second.size () == 1);
    REQUIRE (job_it->second.count (a) == 1);
    REQUIRE (job_it->second.count (b) == 0);

    // Vertex a's tag and exclusive-filter map entries were erased by the
    // failed attempt (both use an erase-before-attempt pattern), but
    // mod_plan () was never reached, so its allocation is still there:
    // the index isn't left pointing at a vertex with nothing real left to
    // clean up.
    REQUIRE_FALSE (db->resource_graph[a].idata.tags.contains (jobid));
    REQUIRE_FALSE (db->resource_graph[a].idata.x_spans.contains (jobid));
    REQUIRE (db->resource_graph[a].schedule.allocations.contains (jobid));
    REQUIRE (db->metadata.vertex_has_job_state (db->resource_graph, jobid, a));
    REQUIRE (db->metadata.verify_job_index (db->resource_graph));

    // Retry revisits exactly the failed vertex and this time succeeds.
    //
    // Note this does *not* depend on "repairing" the bogus x_spans entry:
    // that entry was already erased (unconditionally) during the failed
    // attempt above, and on retry mod_idata () short-circuits via
    // rem_tag () -- the tag is already gone -- without re-examining
    // x_spans (or job2span) at all. What the retry actually purges is the
    // untouched schedule.allocations/schedule.plans state that survived
    // the first failure. This is a deliberate, idempotent design (see the
    // "clearing inconsistency between core and sched" comment on
    // rem_exclusive_filter ()): once a sub-step's map entry is gone,
    // re-running it is defined to be a no-op success rather than an
    // error, which is exactly what makes retries of a partially-failed
    // remove () safe to issue blindly.
    REQUIRE (impl.remove (a, jobid) == 0);
    REQUIRE (db->metadata.by_jobid.find (jobid) == db->metadata.by_jobid.end ());
    REQUIRE_FALSE (db->metadata.vertex_has_job_state (db->resource_graph, jobid, a));
    REQUIRE_FALSE (db->metadata.vertex_has_job_state (db->resource_graph, jobid, b));
    REQUIRE (db->metadata.verify_job_index (db->resource_graph));

    // Removing an already-fully-removed (or never-seen) job is
    // idempotent by design.
    REQUIRE (impl.remove (a, jobid) == 0);
}

TEST_CASE ("by_jobid index remove on a jobid with no index entry is a documented no-op",
           "[by_jobid_index]")
{
    vtx_t a, b;
    auto db = make_bare_graph (a, b);
    auto match = create_match_cb ("first");
    dfu_impl_t impl (db, match);
    // The 2-arg constructor only stashes m_graph_db/m_match; m_graph (the
    // raw pointer cancel_vertex ()/mod_plan () actually dereference) is
    // only set by set_graph_db () -- the same two calls
    // dfu_traverser_t::initialize () makes before using a dfu_impl_t.
    // Skipping this leaves m_graph null and segfaults on first use.
    impl.set_graph_db (db);
    impl.set_match_cb (match);

    REQUIRE (db->metadata.by_jobid.empty ());
    REQUIRE (impl.remove (a, 999) == 0);
    REQUIRE (db->metadata.by_jobid.empty ());
    REQUIRE (db->metadata.verify_job_index (db->resource_graph));
}

TEST_CASE ("a failed purge can leave a vertex indexed with only unkeyed planner state",
           "[by_jobid_index]")
{
    // Companion scenario to the first test case above: here the fault is
    // injected *after* mod_idata () succeeds (tag, exclusive-filter span
    // and aggregate filter are all cleanly purged), but mod_plan () then
    // fails because the vertex's schedule planner was reset out from
    // under it. Because mod_plan () also erases schedule.allocations[jobid]
    // unconditionally before attempting planner_rem_span (), the vertex
    // ends up indexed (purge failed, rc == -1) yet holding *no* remaining
    // job-keyed map state at all -- only unkeyed planner residue (the
    // reset planner itself). vertex_has_job_state () is therefore false
    // for it, and the task's documented caveat applies: the bidirectional
    // verify_job_index () check cannot hold in this transient state, so we
    // skip it here rather than asserting something false. A retry still
    // succeeds because there's nothing left to purge.
    vtx_t a, b;
    auto db = make_bare_graph (a, b);
    auto match = create_match_cb ("first");
    dfu_impl_t impl (db, match);
    // The 2-arg constructor only stashes m_graph_db/m_match; m_graph (the
    // raw pointer cancel_vertex ()/mod_plan () actually dereference) is
    // only set by set_graph_db () -- the same two calls
    // dfu_traverser_t::initialize () makes before using a dfu_impl_t.
    // Skipping this leaves m_graph null and segfaults on first use.
    impl.set_graph_db (db);
    impl.set_match_cb (match);
    const int64_t jobid = 200;

    allocate_job_on_vertex (*db, a, jobid);

    // Corrupt vertex a's schedule planner after the fact: reset it so the
    // span recorded in schedule.allocations[jobid] no longer exists there.
    int64_t base = planner_base_time (db->resource_graph[a].schedule.plans);
    int64_t dur = planner_duration (db->resource_graph[a].schedule.plans);
    REQUIRE (planner_reset (db->resource_graph[a].schedule.plans, base, dur) == 0);

    errno = 0;
    REQUIRE (impl.remove (a, jobid) == -1);

    // Still indexed (the purge failed) ...
    auto job_it = db->metadata.by_jobid.find (jobid);
    REQUIRE (job_it != db->metadata.by_jobid.end ());
    REQUIRE (job_it->second.count (a) == 1);

    // ... but every job-keyed map entry is already gone: mod_idata ()
    // succeeded (erasing tags/x_spans as it went) and mod_plan () erased
    // schedule.allocations[jobid] before discovering the span was invalid.
    REQUIRE_FALSE (db->resource_graph[a].idata.tags.contains (jobid));
    REQUIRE_FALSE (db->resource_graph[a].idata.x_spans.contains (jobid));
    REQUIRE_FALSE (db->resource_graph[a].schedule.allocations.contains (jobid));
    REQUIRE_FALSE (db->metadata.vertex_has_job_state (db->resource_graph, jobid, a));
    // Documented deviation: skip verify_job_index () here. Direction 1 of
    // that check (every indexed pair has vertex_has_job_state () true)
    // does not hold for this vertex right now, by construction of this
    // fault -- that's the scenario this test exists to cover, not a bug.

    // A retry still succeeds: there is nothing left in any job-keyed map
    // for mod_idata ()/mod_plan () to (re-)fail on, so cancel_vertex ()
    // trivially returns 0 and the vertex is finally unindexed.
    REQUIRE (impl.remove (a, jobid) == 0);
    REQUIRE (db->metadata.by_jobid.find (jobid) == db->metadata.by_jobid.end ());
    REQUIRE (db->metadata.verify_job_index (db->resource_graph));
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
