/*****************************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

/* Make sure that a partial release below an exclusive vertex that has no
 * broker rank keeps the time-independent protection of by_excl (): after
 * the planned end of the job, while the job still holds a node below the
 * vertex, an exclusive request for the vertex must be refused, but the
 * released node must be available. resource-query cannot do this test,
 * because it matches at time 0 only. The traverser API accepts the match
 * time, so this test matches after the planned end of the job.
 *
 * The test also examines the two guarantees of remove_exclusive_span ():
 *   R1. The demotion of an exclusive ancestor occurs one time. A second
 *       partial release below the ancestor must not subtract the type of
 *       the ancestor from its proper ancestors again. The per-job
 *       aggregate counts that find () emits are the oracle.
 *   R2. A failure in the demotion must change nothing. The job keeps its
 *       exclusive span and its entry in schedule.allocations, and
 *       by_excl () still refuses an exclusive request for the ancestor.
 */

extern "C" {
#if HAVE_CONFIG_H
#include <config.h>
#endif
}

#include <cerrno>
#include <dlfcn.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <sstream>
#include <string>
#include <jansson.h>
#include "resource/reapi/bindings/c++/reapi_cli.hpp"
#include "resource/policies/base/match_op.h"
#include "src/common/libtap/tap.h"

using namespace Flux::resource_model;
using namespace Flux::resource_model::detail;

// This definition of planner_add_span () in the executable takes the
// place of the definition in the shared library for each call from the
// library (ELF symbol interposition). The test uses it to make the
// creation of a placeholder span fail on demand, which the planner API
// cannot do by itself: a span that holds no resources needs no capacity,
// so a valid interval never fails. All other calls go to the real
// function.
static int fail_zero_span_adds = 0;

extern "C" int64_t planner_add_span (planner_t *ctx,
                                     int64_t start_time,
                                     uint64_t duration,
                                     uint64_t request)
{
    using add_span_f = int64_t (*) (planner_t *, int64_t, uint64_t, uint64_t);
    static add_span_f real = nullptr;
    if (!real) {
        real = reinterpret_cast<add_span_f> (dlsym (RTLD_NEXT, "planner_add_span"));
        if (!real)
            BAIL_OUT ("cannot find the real planner_add_span");
    }
    if (request == 0 && fail_zero_span_adds > 0) {
        fail_zero_span_adds--;
        errno = ENOMEM;
        return -1;
    }
    return real (ctx, start_time, duration, request);
}

// A rack with no broker rank above two nodes with one core each
static const char *rack_jgf = R"({
    "graph": {
        "nodes": [
            {"id": "0", "metadata": {"type": "cluster", "basename": "tiny", "name": "tiny0", "size": 1, "paths": {"containment": "/tiny0"}}},
            {"id": "1", "metadata": {"type": "rack", "basename": "rack", "name": "rack0", "size": 1, "paths": {"containment": "/tiny0/rack0"}}},
            {"id": "2", "metadata": {"type": "node", "basename": "node", "name": "node0", "size": 1, "rank": 0, "paths": {"containment": "/tiny0/rack0/node0"}}},
            {"id": "3", "metadata": {"type": "core", "basename": "core", "name": "core0", "size": 1, "id": 0, "rank": 0, "paths": {"containment": "/tiny0/rack0/node0/core0"}}},
            {"id": "4", "metadata": {"type": "node", "basename": "node", "name": "node1", "size": 1, "rank": 1, "paths": {"containment": "/tiny0/rack0/node1"}}},
            {"id": "5", "metadata": {"type": "core", "basename": "core", "name": "core0", "size": 1, "id": 0, "rank": 1, "paths": {"containment": "/tiny0/rack0/node1/core0"}}}
        ],
        "edges": [
            {"source": "0", "target": "1"},
            {"source": "1", "target": "2"},
            {"source": "2", "target": "3"},
            {"source": "1", "target": "4"},
            {"source": "4", "target": "5"}
        ]
    }
})";

// Two racks with no broker rank. rack0 holds three nodes, rack1 holds one
// node, and each node holds one core. A partial release of a node of
// rack0 demotes rack0 only, thus rack1 shows that the counts of the other
// rack do not change.
static const char *two_rack_jgf = R"({
    "graph": {
        "nodes": [
            {"id": "0", "metadata": {"type": "cluster", "basename": "tiny", "name": "tiny0", "id": 0, "rank": -1, "size": 1, "paths": {"containment": "/tiny0"}}},
            {"id": "1", "metadata": {"type": "rack", "basename": "rack", "name": "rack0", "id": 0, "rank": -1, "size": 1, "paths": {"containment": "/tiny0/rack0"}}},
            {"id": "2", "metadata": {"type": "rack", "basename": "rack", "name": "rack1", "id": 1, "rank": -1, "size": 1, "paths": {"containment": "/tiny0/rack1"}}},
            {"id": "3", "metadata": {"type": "node", "basename": "node", "name": "node0", "id": 0, "rank": 0, "size": 1, "paths": {"containment": "/tiny0/rack0/node0"}}},
            {"id": "4", "metadata": {"type": "core", "basename": "core", "name": "core0", "id": 0, "rank": 0, "size": 1, "paths": {"containment": "/tiny0/rack0/node0/core0"}}},
            {"id": "5", "metadata": {"type": "node", "basename": "node", "name": "node1", "id": 1, "rank": 1, "size": 1, "paths": {"containment": "/tiny0/rack0/node1"}}},
            {"id": "6", "metadata": {"type": "core", "basename": "core", "name": "core0", "id": 0, "rank": 1, "size": 1, "paths": {"containment": "/tiny0/rack0/node1/core0"}}},
            {"id": "7", "metadata": {"type": "node", "basename": "node", "name": "node2", "id": 2, "rank": 2, "size": 1, "paths": {"containment": "/tiny0/rack0/node2"}}},
            {"id": "8", "metadata": {"type": "core", "basename": "core", "name": "core0", "id": 0, "rank": 2, "size": 1, "paths": {"containment": "/tiny0/rack0/node2/core0"}}},
            {"id": "9", "metadata": {"type": "node", "basename": "node", "name": "node3", "id": 3, "rank": 3, "size": 1, "paths": {"containment": "/tiny0/rack1/node3"}}},
            {"id": "10", "metadata": {"type": "core", "basename": "core", "name": "core0", "id": 0, "rank": 3, "size": 1, "paths": {"containment": "/tiny0/rack1/node3/core0"}}}
        ],
        "edges": [
            {"source": "0", "target": "1"},
            {"source": "0", "target": "2"},
            {"source": "1", "target": "3"},
            {"source": "3", "target": "4"},
            {"source": "1", "target": "5"},
            {"source": "5", "target": "6"},
            {"source": "1", "target": "7"},
            {"source": "7", "target": "8"},
            {"source": "2", "target": "9"},
            {"source": "9", "target": "10"}
        ]
    }
})";

// An exclusive rack with one node, for 10 seconds
static const char *excl_rack_jobspec = R"({
    "version": 1,
    "resources": [
        {
            "type": "rack",
            "count": 1,
            "exclusive": true,
            "with": [
                {
                    "type": "node",
                    "count": 1,
                    "with": [
                        {
                            "type": "slot",
                            "count": 1,
                            "label": "task",
                            "with": [{"type": "core", "count": 1}]
                        }
                    ]
                }
            ]
        }
    ],
    "tasks": [{"command": ["sleep", "0"], "slot": "task", "count": {"per_slot": 1}}],
    "attributes": {"system": {"duration": 10.0}}
})";

// Two exclusive racks, for 10 seconds. An exclusive rack holds all of its
// nodes, thus this request takes the four nodes of the two racks.
static const char *two_rack_jobspec = R"({
    "version": 1,
    "resources": [
        {
            "type": "rack",
            "count": 2,
            "exclusive": true,
            "with": [
                {
                    "type": "node",
                    "count": 1,
                    "with": [
                        {
                            "type": "slot",
                            "count": 1,
                            "label": "task",
                            "with": [{"type": "core", "count": 1}]
                        }
                    ]
                }
            ]
        }
    ],
    "tasks": [{"command": ["sleep", "0"], "slot": "task", "count": {"per_slot": 1}}],
    "attributes": {"system": {"duration": 10.0}}
})";

// One core on one node, not exclusive, for 10 seconds
static const char *core_jobspec = R"({
    "version": 1,
    "resources": [
        {
            "type": "node",
            "count": 1,
            "with": [
                {
                    "type": "slot",
                    "count": 1,
                    "label": "task",
                    "with": [{"type": "core", "count": 1}]
                }
            ]
        }
    ],
    "tasks": [{"command": ["sleep", "0"], "slot": "task", "count": {"per_slot": 1}}],
    "attributes": {"system": {"duration": 10.0}}
})";

// One exclusive node, for 10 seconds
static const char *excl_node_jobspec = R"({
    "version": 1,
    "resources": [
        {
            "type": "node",
            "count": 1,
            "exclusive": true,
            "with": [
                {
                    "type": "slot",
                    "count": 1,
                    "label": "task",
                    "with": [{"type": "core", "count": 1}]
                }
            ]
        }
    ],
    "tasks": [{"command": ["sleep", "0"], "slot": "task", "count": {"per_slot": 1}}],
    "attributes": {"system": {"duration": 10.0}}
})";

static const char *rank0_R =
    R"({"version": 1, "execution": {"R_lite": [{"rank": "0", "children": {"core": "0"}}], "starttime": 0, "expiration": 0, "nodelist": ["node0"]}})";
static const char *rank1_R =
    R"({"version": 1, "execution": {"R_lite": [{"rank": "1", "children": {"core": "0"}}], "starttime": 0, "expiration": 0, "nodelist": ["node1"]}})";
static const char *rank2_R =
    R"({"version": 1, "execution": {"R_lite": [{"rank": "2", "children": {"core": "0"}}], "starttime": 0, "expiration": 0, "nodelist": ["node2"]}})";
static const char *rank3_R =
    R"({"version": 1, "execution": {"R_lite": [{"rank": "3", "children": {"core": "0"}}], "starttime": 0, "expiration": 0, "nodelist": ["node3"]}})";

static const char *params_nosched =
    "{\"load_format\": \"jgf\", \"matcher_policy\": \"first\", "
    "\"match_format\": \"rv1_nosched\", \"matcher_name\": \"CA\"}";
static const char *params_rv1 =
    "{\"load_format\": \"jgf\", \"matcher_policy\": \"first\", "
    "\"match_format\": \"rv1\", \"matcher_name\": \"CA\"}";
// The cluster needs a rack pruning filter, because the per-job rack count
// of the cluster shows the demotion of a rack.
static const char *params_prune =
    "{\"load_format\": \"jgf\", \"matcher_policy\": \"first\", "
    "\"match_format\": \"rv1\", \"matcher_name\": \"CA\", "
    "\"prune_filters\": \"ALL:core,ALL:node,cluster:rack\"}";

static resource_query_t *make_rq (const char *graph, const char *params)
{
    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (graph, params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }
    return rq;
}

// Run the traverser for jobspec at the time at. The match API of the
// CLI binding always matches at time 0, so use the traverser directly,
// and record the job as the match API does, so that cancel () finds it.
static int run_at (resource_query_t *rq,
                   const char *jobspec,
                   match_op_t op,
                   int64_t jobid,
                   int64_t &at,
                   std::string &R)
{
    Flux::Jobspec::Jobspec job{jobspec};
    std::stringstream o;
    int64_t now = at;
    int rc = rq->traverser_run (job, op, jobid, at);
    int saved_errno = errno;
    rq->writers->emit (o);
    R = o.str ();
    if (rc == 0 && (op == MATCH_ALLOCATE || op == MATCH_ALLOCATE_ORELSE_RESERVE)) {
        if (at == now)
            rq->set_allocation (jobid);
        else
            rq->set_reservation (jobid);
    }
    errno = saved_errno;
    return rc;
}

// Expand an idset such as "0-2,5" into ranks. Return false if the string
// is not an idset of this form.
static bool expand_idset (const char *s, std::set<int64_t> &ranks)
{
    if (!s)
        return false;
    std::string in{s};
    if (in.empty ())
        return true;
    for (size_t pos = 0;;) {
        size_t comma = in.find (',', pos);
        std::string item =
            in.substr (pos, comma == std::string::npos ? std::string::npos : comma - pos);
        if (item.empty ())
            return false;
        size_t dash = item.find ('-');
        try {
            if (dash == std::string::npos) {
                ranks.insert (std::stoll (item));
            } else {
                int64_t lo = std::stoll (item.substr (0, dash));
                int64_t hi = std::stoll (item.substr (dash + 1));
                if (hi < lo)
                    return false;
                for (int64_t i = lo; i <= hi; ++i)
                    ranks.insert (i);
            }
        } catch (...) {
            return false;
        }
        if (comma == std::string::npos)
            break;
        pos = comma + 1;
    }
    return true;
}

// Parse R and compare the ranks of execution.R_lite with want. A
// substring match cannot do this: the idset "0-1" contains "0" and "1".
static bool R_ranks_are (const std::string &R, const std::set<int64_t> &want)
{
    json_error_t err;
    json_t *o = json_loads (R.c_str (), 0, &err);
    if (!o)
        return false;
    bool good = true;
    std::set<int64_t> got;
    size_t i = 0;
    json_t *entry = NULL;
    json_t *r_lite = json_object_get (json_object_get (o, "execution"), "R_lite");
    if (!json_is_array (r_lite))
        good = false;
    json_array_foreach (r_lite, i, entry) {
        if (!expand_idset (json_string_value (json_object_get (entry, "rank")), got))
            good = false;
    }
    json_decref (o);
    return good && got == want;
}

// Ask find () for the per-job aggregate counts. The jgf writer puts them
// in metadata.agfilter of each vertex that holds an aggregate span of the
// job. The caller owns the result.
static json_t *find_agfilter (void *h, int64_t jobid)
{
    std::string criteria = "jobid-span=" + std::to_string (jobid) + " and agfilter=true";
    json_t *o = nullptr;
    if (reapi_cli_t::find (h, criteria, o, std::string ("jgf")) != 0)
        return nullptr;
    return o;
}

// Return the used count of a type at a containment path in the graph that
// find () emitted. Return -1 if the graph has no such path, or the path
// has no count of that type.
static int64_t agfilter_used (json_t *graph, const char *path, const char *type)
{
    size_t i = 0;
    json_t *v = NULL;
    json_t *nodes = json_object_get (json_object_get (graph, "graph"), "nodes");

    json_array_foreach (nodes, i, v) {
        json_t *md = json_object_get (v, "metadata");
        const char *p =
            json_string_value (json_object_get (json_object_get (md, "paths"), "containment"));
        if (!p || strcmp (p, path) != 0)
            continue;
        // The value has the form "used:X, total:Y"
        const char *counts =
            json_string_value (json_object_get (json_object_get (md, "agfilter"), type));
        if (!counts || strncmp (counts, "used:", 5) != 0)
            return -1;
        return static_cast<int64_t> (strtoll (counts + 5, nullptr, 10));
    }
    return -1;
}

// Compare the per-job counts of core, node and rack at a containment path
// with the expected values. A value of -1 means that the count is absent:
// the vertex has no pruning filter of that type, or the job holds no
// aggregate span on the vertex.
static void check_used (void *h,
                        int64_t jobid,
                        const char *path,
                        int64_t core,
                        int64_t node,
                        int64_t rack,
                        const char *fmt,
                        ...) __attribute__ ((format (printf, 7, 8)));

static void check_used (void *h,
                        int64_t jobid,
                        const char *path,
                        int64_t core,
                        int64_t node,
                        int64_t rack,
                        const char *fmt,
                        ...)
{
    json_t *o = find_agfilter (h, jobid);
    int64_t c = agfilter_used (o, path, "core");
    int64_t n = agfilter_used (o, path, "node");
    int64_t r = agfilter_used (o, path, "rack");
    bool good = (c == core && n == node && r == rack);
    char desc[256];
    va_list ap;

    va_start (ap, fmt);
    vsnprintf (desc, sizeof (desc), fmt, ap);
    va_end (ap);
    ok (good, "%s", desc);
    if (!good)
        diag ("%s: core %jd, node %jd, rack %jd",
              path,
              static_cast<intmax_t> (c),
              static_cast<intmax_t> (n),
              static_cast<intmax_t> (r));
    json_decref (o);
}

// Find the vertex at a containment path. resource_query_t::db is the
// graph that the traverser uses, thus this is the live state.
static vtx_t vertex_at (resource_query_t *rq, const char *path)
{
    auto it = rq->db->metadata.by_path.find (path);
    if (it == rq->db->metadata.by_path.end () || it->second.empty ())
        BAIL_OUT ("the graph has no %s", path);
    return it->second.front ();
}

// Return the count of resources in the schedule span of the job on the
// vertex at a containment path. Return -2 if the vertex holds no entry
// for the job. The entry alone keeps the protection of by_excl (), thus
// a count of 0 shows a demoted exclusive vertex.
static int64_t alloc_span_count (resource_query_t *rq, const char *path, int64_t jobid)
{
    vtx_t v = vertex_at (rq, path);
    auto &sched = rq->db->resource_graph[v].schedule;
    auto it = sched.allocations.find (jobid);
    if (it == sched.allocations.end ())
        return -2;
    return planner_span_resource_count (sched.plans, it->second);
}

// Return true if find () reports no vertex for the criteria.
static bool find_is_empty (void *h, const char *criteria)
{
    json_t *o = nullptr;
    if (reapi_cli_t::find (h, criteria, o, std::string ("jgf")) != 0)
        return false;
    json_t *nodes = json_object_get (json_object_get (o, "graph"), "nodes");
    bool empty = (o == nullptr) || (json_array_size (nodes) == 0);
    json_decref (o);
    return empty;
}

// Job 1 holds the rack exclusively and a partial release freed rank 0.
// Examine the requests of other jobs at a time after the planned end of
// job 1, as if job 1 (or its housekeeping) continues after its walltime.
static void check_overrun (resource_query_t *rq, const char *label)
{
    std::string R;
    int64_t at = 0;
    int rc = 0;

    errno = 0;
    at = 5;
    rc = run_at (rq, excl_rack_jobspec, MATCH_ALLOCATE, 2, at, R);
    ok (rc < 0 && errno == EBUSY, "%s: an exclusive rack request at t=5 is refused", label);

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_rack_jobspec, MATCH_ALLOCATE, 2, at, R);
    ok (rc < 0 && errno == EBUSY,
        "%s: an exclusive rack request at t=20 is refused while job 1 holds node1",
        label);

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_rack_jobspec, MATCH_WITHOUT_ALLOCATING, 2, at, R);
    ok (rc < 0 && errno == EBUSY,
        "%s: an exclusive rack request without allocation at t=20 is refused",
        label);

    // A reservation is optimistic: it can start after the planned end of
    // job 1. But the request must not get the rack now.
    errno = 0;
    at = 20;
    rc = run_at (rq, excl_rack_jobspec, MATCH_ALLOCATE_ORELSE_RESERVE, 2, at, R);
    ok (rc != 0 || at > 20,
        "%s: an exclusive rack request with reservation at t=20 is not allocated now",
        label);
    ok (reapi_cli_t::cancel (rq, 2, true) == 0, "%s: the reservation, if any, is removed", label);

    errno = 0;
    at = 20;
    rc = run_at (rq, core_jobspec, MATCH_ALLOCATE, 3, at, R);
    ok (rc == 0 && R_ranks_are (R, {0}),
        "%s: a core request at t=20 gets the released node 0 only",
        label);
    ok (reapi_cli_t::cancel (rq, 3, true) == 0, "%s: the core job is removed", label);
}

static void test_partial_release_then_overrun ()
{
    resource_query_t *rq = make_rq (rack_jgf, params_nosched);
    void *h = static_cast<void *> (rq);
    std::string R;
    bool reserved = false;
    int64_t at = 0;
    double ov = 0.0;

    ok (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, excl_rack_jobspec, 1, reserved, R, at, ov)
            == 0,
        "live: job 1 gets the exclusive rack at t=0");
    ok (R_ranks_are (R, {0, 1}), "live: the exclusive rack allocation holds both nodes");
    int rc = 0;

    bool full_removal = true;
    ok (reapi_cli_t::cancel (h, 1, rank0_R, false, full_removal) == 0 && !full_removal,
        "live: the partial release of rank 0 is not a full removal");

    check_overrun (rq, "live");

    full_removal = false;
    ok (reapi_cli_t::cancel (h, 1, rank1_R, false, full_removal) == 0 && full_removal,
        "live: the partial release of rank 1 completes the cancel of job 1");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_rack_jobspec, MATCH_ALLOCATE, 4, at, R);
    ok (rc == 0 && R_ranks_are (R, {0, 1}),
        "live: an exclusive rack request at t=20 succeeds after the complete release");
    delete rq;
}

// The same state after a reload: the R of job 1 names rank 0 in free_ranks
static void test_reload_then_overrun ()
{
    resource_query_t *rq = make_rq (rack_jgf, params_rv1);
    void *h = static_cast<void *> (rq);
    std::string R;
    bool reserved = false;
    int64_t at = 0;
    double ov = 0.0;
    int rc = 0;

    ok (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, excl_rack_jobspec, 1, reserved, R, at, ov)
            == 0,
        "reload: job 1 gets the exclusive rack at t=0 (R with the scheduling key)");
    delete rq;

    json_error_t err;
    json_t *o = json_loads (R.c_str (), 0, &err);
    json_t *scheduling = o ? json_object_get (o, "scheduling") : nullptr;
    if (!scheduling)
        BAIL_OUT ("R has no scheduling key");
    json_object_set_new (scheduling, "free_ranks", json_string ("0"));
    char *s = json_dumps (o, JSON_COMPACT);
    std::string R_free0 = s;
    free (s);
    json_decref (o);

    rq = make_rq (rack_jgf, params_rv1);
    h = static_cast<void *> (rq);
    std::string R_out;
    at = 0;
    ok (reapi_cli_t::update_allocate (h, 1, R_free0, at, ov, R_out) == 0,
        "reload: job 1 is reloaded with rank 0 in free_ranks");
    ok (R_ranks_are (R_out, {1}), "reload: the reloaded allocation holds node 1 only");

    check_overrun (rq, "reload");

    // The replayed state must also complete: the release of the last rank
    // of the job removes the job, and the rack is free again.
    bool full_removal = false;
    ok (reapi_cli_t::cancel (h, 1, rank1_R, false, full_removal) == 0 && full_removal,
        "reload: the release of rank 1 completes the cancel of job 1");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_rack_jobspec, MATCH_ALLOCATE, 2, at, R);
    ok (rc == 0 && R_ranks_are (R, {0, 1}),
        "reload: an exclusive rack request at t=20 gets both nodes");
    ok (reapi_cli_t::cancel (h, 2, true) == 0, "reload: job 2 is removed");
    ok (find_is_empty (h, "jobid-tag=1"), "reload: no vertex keeps a tag of job 1");
    ok (find_is_empty (h, "sched-now=allocated"), "reload: no vertex stays allocated");
    delete rq;
}

// R1: the demotion of an exclusive rack occurs one time. The per-job rack
// count of the cluster is the oracle. Before the fix, each partial
// release below rack0 subtracted the rack again, and the count reached 0
// while the job still held rack1.
static void test_two_racks_demote_one_time ()
{
    resource_query_t *rq = make_rq (two_rack_jgf, params_prune);
    void *h = static_cast<void *> (rq);
    std::string R;
    bool reserved = false;
    bool full_removal = true;
    int64_t at = 0;
    double ov = 0.0;

    ok (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, two_rack_jobspec, 1, reserved, R, at, ov)
            == 0,
        "two racks: job 1 gets both racks at t=0");
    ok (R_ranks_are (R, {0, 1, 2, 3}), "two racks: the allocation holds the four nodes");
    check_used (h, 1, "/tiny0", 4, 4, 2, "two racks: the cluster holds 4 cores, 4 nodes, 2 racks");
    check_used (h, 1, "/tiny0/rack1", 1, 1, -1, "two racks: rack1 holds 1 core and 1 node");
    ok (alloc_span_count (rq, "/tiny0/rack0", 1) == 1, "two racks: rack0 holds an exclusive span");

    full_removal = true;
    ok (reapi_cli_t::cancel (h, 1, rank0_R, false, full_removal) == 0 && !full_removal,
        "two racks: the release of rank 0 is not a full removal");
    check_used (h, 1, "/tiny0", 3, 3, 1, "two racks: the release of rank 0 demotes rack0");
    check_used (h, 1, "/tiny0/rack1", 1, 1, -1, "two racks: rack1 is the same after rank 0");
    ok (alloc_span_count (rq, "/tiny0/rack0", 1) == 0,
        "two racks: rack0 keeps an entry with no resources");

    full_removal = true;
    ok (reapi_cli_t::cancel (h, 1, rank1_R, false, full_removal) == 0 && !full_removal,
        "two racks: the release of rank 1 is not a full removal");
    check_used (h, 1, "/tiny0", 2, 2, 1, "two racks: the release of rank 1 keeps 1 rack");
    check_used (h, 1, "/tiny0/rack1", 1, 1, -1, "two racks: rack1 is the same after rank 1");
    ok (alloc_span_count (rq, "/tiny0/rack0", 1) == 0,
        "two racks: the second release keeps the entry of rack0");

    full_removal = true;
    ok (reapi_cli_t::cancel (h, 1, rank2_R, false, full_removal) == 0 && !full_removal,
        "two racks: the release of rank 2 empties rack0");
    check_used (h, 1, "/tiny0", 1, 1, 1, "two racks: the cluster keeps the count of rack1");
    check_used (h, 1, "/tiny0/rack1", 1, 1, -1, "two racks: rack1 is the same after rank 2");

    full_removal = false;
    ok (reapi_cli_t::cancel (h, 1, rank3_R, false, full_removal) == 0 && full_removal,
        "two racks: the release of rank 3 completes the cancel of job 1");
    ok (find_is_empty (h, "jobid-alloc=1"), "two racks: no vertex keeps a span of job 1");
    delete rq;
}

// R1 after a replay: the reader demotes rack0 while it reloads job 1, and
// a later partial release below rack0 must not demote it again.
static void test_two_racks_replay (bool remove_rank0)
{
    const char *label = remove_rank0 ? "replay absent" : "replay present";
    resource_query_t *rq = make_rq (two_rack_jgf, params_prune);
    void *h = static_cast<void *> (rq);
    std::string R;
    bool reserved = false;
    bool full_removal = true;
    int64_t at = 0;
    double ov = 0.0;

    if (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, two_rack_jobspec, 1, reserved, R, at, ov)
        != 0)
        BAIL_OUT ("the two-rack match for the replay failed");
    delete rq;

    json_error_t err;
    json_t *o = json_loads (R.c_str (), 0, &err);
    json_t *scheduling = o ? json_object_get (o, "scheduling") : nullptr;
    if (!scheduling)
        BAIL_OUT ("the two-rack R has no scheduling key");
    json_object_set_new (scheduling, "free_ranks", json_string ("0"));
    char *s = json_dumps (o, JSON_COMPACT);
    std::string R_free0 = s;
    free (s);
    json_decref (o);

    rq = make_rq (two_rack_jgf, params_prune);
    h = static_cast<void *> (rq);
    // A shrink removes the freed rank from the graph. Then the reader
    // cannot walk up from the rank, and it uses the paths of the JGF of
    // the job to find the ancestors to demote.
    if (remove_rank0)
        ok (reapi_cli_t::remove_subgraph (h, "/tiny0/rack0/node0") == 0,
            "%s: a shrink removes rank 0 from the graph",
            label);

    std::string R_out;
    at = 0;
    ok (reapi_cli_t::update_allocate (h, 1, R_free0, at, ov, R_out) == 0,
        "%s: job 1 is reloaded with rank 0 in free_ranks",
        label);
    ok (R_ranks_are (R_out, {1, 2, 3}), "%s: the reloaded allocation holds three nodes", label);
    check_used (h, 1, "/tiny0", 3, 3, 1, "%s: the reload demotes rack0", label);
    check_used (h, 1, "/tiny0/rack1", 1, 1, -1, "%s: rack1 holds 1 core and 1 node", label);

    full_removal = true;
    ok (reapi_cli_t::cancel (h, 1, rank1_R, false, full_removal) == 0 && !full_removal,
        "%s: the release of rank 1 is not a full removal",
        label);
    check_used (h, 1, "/tiny0", 2, 2, 1, "%s: the release of rank 1 keeps 1 rack", label);

    full_removal = true;
    ok (reapi_cli_t::cancel (h, 1, rank2_R, false, full_removal) == 0 && !full_removal,
        "%s: the release of rank 2 empties rack0",
        label);
    check_used (h, 1, "/tiny0", 1, 1, 1, "%s: the cluster keeps the count of rack1", label);

    full_removal = false;
    ok (reapi_cli_t::cancel (h, 1, rank3_R, false, full_removal) == 0 && full_removal,
        "%s: the release of rank 3 completes the cancel of job 1",
        label);
    ok (find_is_empty (h, "jobid-tag=1"), "%s: no vertex keeps a tag of job 1", label);
    delete rq;
}

// R2: a failure in the demotion of an exclusive ancestor changes nothing.
// The only failure that the public API can force is a bad span id in
// schedule.allocations: remove_exclusive_span () validates the old span
// first, and returns before it adds or removes a span. The other branch,
// a failure of the add, has the same guarantee by construction, because
// the add occurs before the removal of the old span.
// resource_query_t::db is the graph that the traverser uses, thus a
// change here is a change of the live state of the traverser.
static void test_failed_demotion_keeps_the_state ()
{
    resource_query_t *rq = make_rq (rack_jgf, params_nosched);
    void *h = static_cast<void *> (rq);
    std::string R;
    bool reserved = false;
    bool full_removal = true;
    int64_t at = 0;
    double ov = 0.0;
    int rc = 0;

    if (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, excl_rack_jobspec, 1, reserved, R, at, ov)
        != 0)
        BAIL_OUT ("the exclusive rack match for the fault test failed");

    vtx_t rack = vertex_at (rq, "/tiny0/rack0");
    planner_t *plans = rq->db->resource_graph[rack].schedule.plans;
    auto &allocations = rq->db->resource_graph[rack].schedule.allocations;
    auto span_it = allocations.find (1);
    if (span_it == allocations.end ())
        BAIL_OUT ("job 1 holds no span on rack0");
    int64_t saved = span_it->second;
    int64_t bogus = saved + 424242;
    span_it->second = bogus;

    errno = 0;
    full_removal = true;
    rc = reapi_cli_t::cancel (h, 1, rank0_R, false, full_removal);
    ok (rc != 0, "fault: the partial release reports the error");
    ok (allocations.size () == 1 && allocations.count (1) == 1 && allocations.at (1) == bogus,
        "fault: the entry of job 1 on the rack does not change");
    ok (planner_span_resource_count (plans, saved) == 1,
        "fault: the exclusive span of job 1 on the rack stays");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_rack_jobspec, MATCH_ALLOCATE, 2, at, R);
    ok (rc < 0 && errno == EBUSY, "fault: an exclusive rack request at t=20 is still refused");

    // The failed partial release already purged the vertices of rank 0 and
    // reduced the aggregate span of the rack, thus a retry of the same
    // partial release cannot succeed. The full cancel is the recovery.
    allocations[1] = saved;
    ok (reapi_cli_t::cancel (h, 1, false) == 0, "fault: the full cancel of job 1 succeeds");
    ok (find_is_empty (h, "jobid-tag=1"), "fault: no vertex keeps a tag of job 1");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_rack_jobspec, MATCH_ALLOCATE, 3, at, R);
    ok (rc == 0 && R_ranks_are (R, {0, 1}),
        "fault: an exclusive rack request at t=20 succeeds after the full cancel");
    delete rq;
}

// The creation of the placeholder span fails. The rack must keep the
// exclusive span of job 1 and the entry, the partial release must report
// the error, and an exclusive rack request must stay refused after the
// planned end of job 1. The full cancel then removes job 1.
static void test_failed_placeholder_keeps_the_span ()
{
    resource_query_t *rq = make_rq (rack_jgf, params_nosched);
    void *h = static_cast<void *> (rq);
    std::string R;
    bool reserved = false;
    bool full_removal = true;
    int64_t at = 0;
    double ov = 0.0;
    int rc = 0;

    if (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, excl_rack_jobspec, 1, reserved, R, at, ov)
        != 0)
        BAIL_OUT ("the exclusive rack match for the placeholder fault test failed");

    vtx_t rack = vertex_at (rq, "/tiny0/rack0");
    planner_t *plans = rq->db->resource_graph[rack].schedule.plans;
    auto &allocations = rq->db->resource_graph[rack].schedule.allocations;
    auto span_it = allocations.find (1);
    if (span_it == allocations.end ())
        BAIL_OUT ("job 1 holds no span on rack0");
    int64_t saved = span_it->second;

    fail_zero_span_adds = 1;
    errno = 0;
    full_removal = true;
    rc = reapi_cli_t::cancel (h, 1, rank0_R, false, full_removal);
    ok (rc != 0 && fail_zero_span_adds == 0,
        "placeholder fault: the partial release reports the error");
    ok (allocations.size () == 1 && allocations.count (1) == 1 && allocations.at (1) == saved,
        "placeholder fault: the entry of job 1 on the rack keeps the old span");
    ok (planner_span_resource_count (plans, saved) == 1,
        "placeholder fault: the old exclusive span of job 1 stays");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_rack_jobspec, MATCH_ALLOCATE, 2, at, R);
    ok (rc < 0 && errno == EBUSY,
        "placeholder fault: an exclusive rack request at t=20 is still refused");

    // The old span still holds the rack, so by_avail () does not examine
    // the released node: the state is the conservative one. The full
    // cancel is the recovery.
    ok (reapi_cli_t::cancel (h, 1, false) == 0, "placeholder fault: the full cancel succeeds");
    ok (find_is_empty (h, "jobid-tag=1"), "placeholder fault: no vertex keeps a tag of job 1");
    ok (find_is_empty (h, "sched-now=allocated"), "placeholder fault: no vertex stays allocated");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_rack_jobspec, MATCH_ALLOCATE, 3, at, R);
    ok (rc == 0 && R_ranks_are (R, {0, 1}),
        "placeholder fault: an exclusive rack request at t=20 succeeds after the full cancel");
    delete rq;
}

int main (int argc, char *argv[])
{
    plan (NO_PLAN);
    test_partial_release_then_overrun ();
    test_reload_then_overrun ();
    test_two_racks_demote_one_time ();
    test_two_racks_replay (false);
    test_two_racks_replay (true);
    test_failed_demotion_keeps_the_state ();
    test_failed_placeholder_keeps_the_span ();

    // Known limitation, not part of this fix: when an exclusive request
    // for a rack includes a node that an overrunning job holds exclusively,
    // the match walk refuses that node, but the update walk allocates all
    // the nodes of the rack to the new job.
    {
        resource_query_t *rq = make_rq (rack_jgf, params_nosched);
        void *h = static_cast<void *> (rq);
        std::string R;
        bool reserved = false;
        int64_t at = 0;
        double ov = 0.0;
        ok (reapi_cli_t::match_allocate (h,
                                         MATCH_ALLOCATE,
                                         excl_node_jobspec,
                                         1,
                                         reserved,
                                         R,
                                         at,
                                         ov)
                == 0,
            "shadow: job 1 gets one exclusive node at t=0");
        errno = 0;
        at = 20;
        int rc = run_at (rq, excl_rack_jobspec, MATCH_ALLOCATE, 2, at, R);
        todo ("an exclusive rack request allocates a node that an overrunning job holds");
        ok (rc < 0 && errno == EBUSY,
            "shadow: an exclusive rack request at t=20 is refused while job 1 holds a node");
        end_todo;
        delete rq;
    }

    done_testing ();
    return 0;
}
