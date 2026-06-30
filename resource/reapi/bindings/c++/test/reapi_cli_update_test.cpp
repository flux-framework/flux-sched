/*****************************************************************************\
 * Copyright 2025 Lawrence Livermore National Security, LLC
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
#include "resource/reapi/bindings/c++/reapi_cli.hpp"
#include "resource/policies/base/match_op.h"
#include "resource/store/resource_graph_store.hpp"
#include "resource/schema/resource_data.hpp"
#include "resource/schema/data_std.hpp"
#include "src/common/libtap/tap.h"

using namespace Flux::resource_model;
using namespace Flux::resource_model::detail;

static const char *tiny_jgf = R"({
    "graph": {
        "nodes": [
            {"id": "0", "metadata": {"type": "cluster", "basename": "tiny", "name": "tiny0", "size": 1, "paths": {"containment": "/tiny0"}}},
            {"id": "1", "metadata": {"type": "node", "basename": "node", "name": "node0", "size": 1, "rank": 0, "paths": {"containment": "/tiny0/node0"}}},
            {"id": "2", "metadata": {"type": "core", "basename": "core", "name": "core0", "size": 1, "id": 0, "rank": 0, "paths": {"containment": "/tiny0/node0/core0"}}}
        ],
        "edges": [{"source": "0", "target": "1"}, {"source": "1", "target": "2"}]
    }
})";

// match_format must be rv1 so the emitted R carries the "scheduling" key that
// update_allocate replays via the JGF reader.
static const char *tiny_params = R"({
    "load_format": "jgf",
    "matcher_policy": "high",
    "match_format": "rv1",
    "matcher_name": "CA"
})";

static const char *simple_jobspec = R"({
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
    "attributes": {"system": {"duration": 60.0}}
})";

// update_allocate replays an allocation R (captured from match_allocate) into a
// fresh, allocation-free graph -- the scheduler-restart recovery path. After
// replay the resources must be held under the same jobid, exactly as if they
// had been matched.
static int test_update_replays_allocation ()
{
    // Original graph: match-allocate to capture an rv1 R + jobid.
    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (tiny_jgf, tiny_params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }
    void *h = static_cast<void *> (rq);

    uint64_t jobid = 1;
    bool reserved = false;
    std::string R;
    int64_t at = 0;
    double ov = 0.0;

    int rc =
        reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, simple_jobspec, jobid, reserved, R, at, ov);
    if (rc < 0) {
        delete rq;
        BAIL_OUT ("match_allocate failed");
    }
    ok (rc == 0 && !R.empty (), "match_allocate produced an rv1 R to replay");

    // Fresh graph with no allocation history, as after a scheduler restart.
    resource_query_t *fresh = nullptr;
    try {
        fresh = new resource_query_t (tiny_jgf, tiny_params);
    } catch (...) {
        delete rq;
        BAIL_OUT ("couldn't create fresh resource_query_t");
    }
    void *hf = static_cast<void *> (fresh);

    // Replay the captured R under the same jobid.
    int64_t at_out = 0;
    double ov_out = 0.0;
    std::string R_out;
    rc = reapi_cli_t::update_allocate (hf, jobid, R, at_out, ov_out, R_out);
    ok (rc == 0, "update_allocate replayed the allocation into the fresh graph");
    ok (!R_out.empty (), "update_allocate emitted an R for the replayed allocation");

    // The replayed jobid must now be tracked (set_job bookkeeping).
    std::string mode;
    bool rsv = false;
    int64_t at_info = 0;
    double ov_info = 0.0;
    rc = reapi_cli_t::info (hf, jobid, mode, rsv, at_info, ov_info);
    ok (rc == 0, "info finds the replayed jobid on the fresh graph");

    // Those resources are now busy: a fresh match for the same spec must fail.
    uint64_t jobid2 = 2;
    std::string R2;
    errno = 0;
    rc = reapi_cli_t::match_allocate (hf,
                                      MATCH_ALLOCATE,
                                      simple_jobspec,
                                      jobid2,
                                      reserved,
                                      R2,
                                      at,
                                      ov);
    ok (rc == -1 && errno == EBUSY,
        "re-match after replay fails with EBUSY (replay is holding the resource)");

    delete fresh;
    delete rq;
    return 0;
}

// update_allocate must refuse to clobber a jobid the graph already tracks.
static int test_update_rejects_duplicate ()
{
    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (tiny_jgf, tiny_params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }
    void *h = static_cast<void *> (rq);

    uint64_t jobid = 1;
    bool reserved = false;
    std::string R;
    int64_t at = 0;
    double ov = 0.0;
    int rc =
        reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, simple_jobspec, jobid, reserved, R, at, ov);
    if (rc < 0) {
        delete rq;
        BAIL_OUT ("match_allocate failed");
    }

    resource_query_t *fresh = nullptr;
    try {
        fresh = new resource_query_t (tiny_jgf, tiny_params);
    } catch (...) {
        delete rq;
        BAIL_OUT ("couldn't create fresh resource_query_t");
    }
    void *hf = static_cast<void *> (fresh);

    int64_t at_out = 0;
    double ov_out = 0.0;
    std::string R_out;
    rc = reapi_cli_t::update_allocate (hf, jobid, R, at_out, ov_out, R_out);
    if (rc < 0) {
        delete fresh;
        delete rq;
        BAIL_OUT ("first update_allocate failed");
    }

    // Replaying the same jobid again must be refused.
    errno = 0;
    rc = reapi_cli_t::update_allocate (hf, jobid, R, at_out, ov_out, R_out);
    ok (rc == -1 && errno == EEXIST, "duplicate update_allocate fails with EEXIST");

    delete fresh;
    delete rq;
    return 0;
}

// update_allocate must reject an empty R rather than crash.
static int test_update_rejects_empty_R ()
{
    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (tiny_jgf, tiny_params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }
    void *h = static_cast<void *> (rq);

    uint64_t jobid = 1;
    int64_t at_out = 0;
    double ov_out = 0.0;
    std::string R_out;
    errno = 0;
    int rc = reapi_cli_t::update_allocate (h, jobid, "", at_out, ov_out, R_out);
    ok (rc == -1 && errno == EINVAL, "update_allocate with empty R fails with EINVAL");

    delete rq;
    return 0;
}

// Null handle hits the !rq half of the EINVAL guard.
static int test_update_rejects_null_handle ()
{
    uint64_t jobid = 1;
    int64_t at_out = 0;
    double ov_out = 0.0;
    std::string R_out;
    errno = 0;
    int rc = reapi_cli_t::update_allocate (nullptr, jobid, "{}", at_out, ov_out, R_out);
    ok (rc == -1 && errno == EINVAL, "update_allocate with null handle fails with EINVAL");
    return 0;
}

// A bare JGF allocation R (no rv1 "scheduling"/"execution" envelope) exercises
// the else branches and the default-duration fallback. We capture an rv1 R,
// pull out just its "scheduling" subgraph, and replay that bare graph.
static int test_update_replays_bare_jgf ()
{
    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (tiny_jgf, tiny_params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }
    void *h = static_cast<void *> (rq);

    uint64_t jobid = 1;
    bool reserved = false;
    std::string R;
    int64_t at = 0;
    double ov = 0.0;
    int rc =
        reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, simple_jobspec, jobid, reserved, R, at, ov);
    if (rc < 0) {
        delete rq;
        BAIL_OUT ("match_allocate failed");
    }

    // Extract the bare "scheduling" subgraph from the rv1 R.
    std::string bare_R;
    json_error_t jerr;
    json_t *root = json_loads (R.c_str (), 0, &jerr);
    if (root) {
        json_t *sched = json_object_get (root, "scheduling");
        if (sched) {
            char *s = json_dumps (sched, JSON_COMPACT);
            if (s) {
                bare_R = s;
                free (s);
            }
        }
        json_decref (root);
    }
    if (bare_R.empty ()) {
        delete rq;
        BAIL_OUT ("could not extract scheduling subgraph from rv1 R");
    }

    resource_query_t *fresh = nullptr;
    try {
        fresh = new resource_query_t (tiny_jgf, tiny_params);
    } catch (...) {
        delete rq;
        BAIL_OUT ("couldn't create fresh resource_query_t");
    }
    void *hf = static_cast<void *> (fresh);

    int64_t at_out = 0;
    double ov_out = 0.0;
    std::string R_out;
    rc = reapi_cli_t::update_allocate (hf, jobid, bare_R, at_out, ov_out, R_out);
    ok (rc == 0, "update_allocate replays a bare JGF graph (no rv1 envelope)");

    delete fresh;
    delete rq;
    return 0;
}

// A syntactically valid JGF that references a vertex absent from the graph makes
// the reader/traverser fail, exercising the rc < 0 error branch.
static int test_update_traverser_failure ()
{
    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (tiny_jgf, tiny_params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }
    void *h = static_cast<void *> (rq);

    // Valid JSON/JGF shape, but the vertex ids/paths don't exist in tiny_jgf.
    static const char *bogus_R = R"({
        "graph": {
            "nodes": [
                {"id": "999", "metadata": {"type": "core", "basename": "core", "name": "core99", "size": 1, "id": 99, "rank": 0, "exclusive": true, "paths": {"containment": "/tiny0/node0/core99"}}}
            ],
            "edges": []
        }
    })";

    uint64_t jobid = 1;
    int64_t at_out = 0;
    double ov_out = 0.0;
    std::string R_out;
    errno = 0;
    int rc = reapi_cli_t::update_allocate (h, jobid, bogus_R, at_out, ov_out, R_out);
    ok (rc == -1, "update_allocate fails when the replay graph does not match");

    delete rq;
    return 0;
}

int main (int argc, char *argv[])
{
    plan (10);

    test_update_replays_allocation ();
    test_update_rejects_duplicate ();
    test_update_rejects_empty_R ();
    test_update_rejects_null_handle ();
    test_update_replays_bare_jgf ();
    test_update_traverser_failure ();

    done_testing ();
    return 0;
}
