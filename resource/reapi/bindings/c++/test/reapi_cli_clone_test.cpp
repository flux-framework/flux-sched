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

static const char *tiny_params = R"({
    "load_format": "jgf",
    "matcher_policy": "high",
    "match_format": "rv1",
    "matcher_name": "CA"
})";

static const char *two_node_jgf = R"({
    "graph": {
        "nodes": [
            {"id": "0", "metadata": {"type": "cluster", "basename": "tiny", "name": "tiny0", "size": 1, "paths": {"containment": "/tiny0"}}},
            {"id": "1", "metadata": {"type": "node", "basename": "node", "name": "node0", "size": 1, "rank": 0, "paths": {"containment": "/tiny0/node0"}}},
            {"id": "2", "metadata": {"type": "core", "basename": "core", "name": "core0", "size": 1, "id": 0, "rank": 0, "paths": {"containment": "/tiny0/node0/core0"}}},
            {"id": "3", "metadata": {"type": "node", "basename": "node", "name": "node1", "size": 1, "rank": 1, "paths": {"containment": "/tiny0/node1"}}},
            {"id": "4", "metadata": {"type": "core", "basename": "core", "name": "core0", "size": 1, "id": 0, "rank": 1, "paths": {"containment": "/tiny0/node1/core0"}}}
        ],
        "edges": [
            {"source": "0", "target": "1"},
            {"source": "1", "target": "2"},
            {"source": "0", "target": "3"},
            {"source": "3", "target": "4"}
        ]
    }
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

static const char *short_jobspec = R"({
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

static const char *long_jobspec = R"({
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
    "attributes": {"system": {"duration": 120.0}}
})";

static int test_clone_basic ()
{
    // Setup: create original context
    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (tiny_jgf, tiny_params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }

    // Test: copy constructor should succeed
    resource_query_t *clone = nullptr;
    bool copy_succeeded = false;
    try {
        clone = new resource_query_t (*rq);
        copy_succeeded = true;
    } catch (...) {
        copy_succeeded = false;
    }

    ok (copy_succeeded, "copy constructor succeeded");
    ok (clone != nullptr && clone != rq, "clone is a different object");

    delete clone;
    delete rq;
    return 0;
}

static int test_clone_independence ()
{
    // Setup: create original and clone
    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (tiny_jgf, tiny_params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }

    resource_query_t *clone = nullptr;
    try {
        clone = new resource_query_t (*rq);
    } catch (...) {
        delete rq;
        BAIL_OUT ("copy constructor failed");
    }

    void *h_orig = static_cast<void *> (rq);
    void *h_clone = static_cast<void *> (clone);

    // Test: allocate on clone
    uint64_t jobid1 = 1;
    bool reserved = false;
    std::string R1;
    int64_t at = 0;
    double ov = 0.0;

    int rc = reapi_cli_t::match_allocate (h_clone,
                                          MATCH_ALLOCATE,
                                          simple_jobspec,
                                          jobid1,
                                          reserved,
                                          R1,
                                          at,
                                          ov);
    ok (rc == 0, "allocation on clone succeeded");

    // Test: allocate same resources on original - should succeed (clone is independent)
    uint64_t jobid2 = 2;
    std::string R2;
    rc = reapi_cli_t::match_allocate (h_orig,
                                      MATCH_ALLOCATE,
                                      simple_jobspec,
                                      jobid2,
                                      reserved,
                                      R2,
                                      at,
                                      ov);
    ok (rc == 0, "allocation on original succeeded (clone is independent)");

    delete clone;
    delete rq;
    return 0;
}

static int test_clone_copies_allocations ()
{
    // Setup: create original context
    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (tiny_jgf, tiny_params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }

    void *h = static_cast<void *> (rq);

    // Setup: make an allocation on the original
    uint64_t jobid1 = 1;
    bool reserved = false;
    std::string R1;
    int64_t at = 0;
    double ov = 0.0;

    int rc = reapi_cli_t::match_allocate (h,
                                          MATCH_ALLOCATE,
                                          simple_jobspec,
                                          jobid1,
                                          reserved,
                                          R1,
                                          at,
                                          ov);
    if (rc < 0) {
        delete rq;
        BAIL_OUT ("match_allocate failed");
    }

    // Test: clone after allocation
    resource_query_t *clone = nullptr;
    try {
        clone = new resource_query_t (*rq);
    } catch (...) {
        delete rq;
        BAIL_OUT ("copy constructor failed");
    }

    void *h_clone = static_cast<void *> (clone);

    // Test: try to allocate same resources on clone - should fail (resources busy)
    uint64_t jobid2 = 2;
    std::string R2;
    errno = 0;
    rc = reapi_cli_t::match_allocate (h_clone,
                                      MATCH_ALLOCATE,
                                      simple_jobspec,
                                      jobid2,
                                      reserved,
                                      R2,
                                      at,
                                      ov);
    ok (rc == -1 && errno == EBUSY, "allocation on clone fails with EBUSY (allocation was copied)");

    // Test: cancel on clone shouldn't affect original
    rc = reapi_cli_t::cancel (h_clone, jobid1, false);
    ok (rc == 0, "cancel on clone succeeded");

    // Test: original should still have the allocation
    std::string mode;
    int64_t at_out;
    double ov_out;
    rc = reapi_cli_t::info (h, jobid1, mode, reserved, at_out, ov_out);
    ok (rc == 0, "original still has allocation after cancel on clone");

    delete clone;
    delete rq;
    return 0;
}

static int test_clone_copies_reservations ()
{
    // Setup: create original context
    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (tiny_jgf, tiny_params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }

    void *h = static_cast<void *> (rq);

    // Setup: make a reservation on the original
    uint64_t jobid1 = 1;
    bool reserved = false;
    std::string R1;
    int64_t at = 0;
    double ov = 0.0;

    int rc = reapi_cli_t::match_allocate (h,
                                          MATCH_ALLOCATE_ORELSE_RESERVE,
                                          simple_jobspec,
                                          jobid1,
                                          reserved,
                                          R1,
                                          at,
                                          ov);
    if (rc < 0) {
        delete rq;
        BAIL_OUT ("match_allocate failed");
    }

    // Test: clone after allocation/reservation
    resource_query_t *clone = nullptr;
    try {
        clone = new resource_query_t (*rq);
    } catch (...) {
        delete rq;
        BAIL_OUT ("copy constructor failed");
    }

    void *h_clone = static_cast<void *> (clone);

    // Test: job should exist in the clone
    std::string mode;
    int64_t at_out;
    double ov_out;
    rc = reapi_cli_t::info (h_clone, jobid1, mode, reserved, at_out, ov_out);
    ok (rc == 0, "job exists in clone after copy");

    delete clone;
    delete rq;
    return 0;
}

static int test_clone_copies_up_down_status ()
{
    resource_query_t *rq = new resource_query_t (tiny_jgf, tiny_params);
    if (rq->get_resource_query_err_msg () != "") {
        delete rq;
        BAIL_OUT ("resource_query_t constructor failed");
    }

    void *h = static_cast<void *> (rq);

    // Set rank 0 to DOWN using public API
    int rc = reapi_cli_t::set_rank_status (h, "0", resource_pool_t::status_t::DOWN);
    ok (rc == 0, "set rank 0 to DOWN succeeded");

    // Verify it's down
    resource_pool_t::status_t status;
    rc = reapi_cli_t::get_rank_status (h, "0", status);
    ok (rc == 0 && status == resource_pool_t::status_t::DOWN, "rank 0 is DOWN");

    // Clone the resource query
    resource_query_t *clone = new resource_query_t (*rq);
    ok (clone != nullptr, "clone succeeded");

    void *h_clone = static_cast<void *> (clone);

    // Verify the clone has rank 0 DOWN
    rc = reapi_cli_t::get_rank_status (h_clone, "0", status);
    ok (rc == 0 && status == resource_pool_t::status_t::DOWN,
        "clone preserves status: rank 0 is DOWN in clone");

    // Verify independence: set rank 0 to UP in clone
    rc = reapi_cli_t::set_rank_status (h_clone, "0", resource_pool_t::status_t::UP);
    ok (rc == 0, "set rank 0 to UP in clone succeeded");

    rc = reapi_cli_t::get_rank_status (h_clone, "0", status);
    ok (rc == 0 && status == resource_pool_t::status_t::UP, "rank 0 is UP in clone");

    // Verify original is still DOWN (independence)
    rc = reapi_cli_t::get_rank_status (h, "0", status);
    ok (rc == 0 && status == resource_pool_t::status_t::DOWN,
        "original rank 0 still DOWN (clone is independent)");

    delete clone;
    delete rq;
    return 0;
}

static int test_clone_with_multiple_jobs ()
{
    // Setup: create context with multiple nodes
    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (two_node_jgf, tiny_params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t with two nodes");
    }

    void *h = static_cast<void *> (rq);

    // Allocate first job on one node
    uint64_t jobid1 = 1;
    bool reserved = false;
    std::string R1;
    int64_t at = 0;
    double ov = 0.0;

    int rc = reapi_cli_t::match_allocate (h,
                                          MATCH_ALLOCATE,
                                          simple_jobspec,
                                          jobid1,
                                          reserved,
                                          R1,
                                          at,
                                          ov);
    if (rc < 0) {
        delete rq;
        BAIL_OUT ("first allocation failed");
    }

    // Allocate second job on other node
    uint64_t jobid2 = 2;
    std::string R2;
    rc = reapi_cli_t::match_allocate (h,
                                      MATCH_ALLOCATE,
                                      simple_jobspec,
                                      jobid2,
                                      reserved,
                                      R2,
                                      at,
                                      ov);
    if (rc < 0) {
        delete rq;
        BAIL_OUT ("second allocation failed");
    }
    ok (true, "allocated two jobs on original");

    // Clone with multiple allocations
    resource_query_t *clone = nullptr;
    try {
        clone = new resource_query_t (*rq);
    } catch (...) {
        delete rq;
        BAIL_OUT ("clone failed");
    }

    void *h_clone = static_cast<void *> (clone);

    // Verify both allocations are present in clone
    std::string mode;
    int64_t at_out;
    double ov_out;
    rc = reapi_cli_t::info (h_clone, jobid1, mode, reserved, at_out, ov_out);
    ok (rc == 0, "job 1 exists in clone");

    rc = reapi_cli_t::info (h_clone, jobid2, mode, reserved, at_out, ov_out);
    ok (rc == 0, "job 2 exists in clone");

    // Cancel one job in clone, verify original unaffected
    rc = reapi_cli_t::cancel (h_clone, jobid1, false);
    ok (rc == 0, "canceled job 1 in clone");

    rc = reapi_cli_t::info (h, jobid1, mode, reserved, at_out, ov_out);
    ok (rc == 0, "job 1 still exists in original after cancel in clone");

    rc = reapi_cli_t::info (h, jobid2, mode, reserved, at_out, ov_out);
    ok (rc == 0, "job 2 still exists in original");

    delete clone;
    delete rq;
    return 0;
}

static int test_clone_of_clone ()
{
    // Setup: create original
    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (tiny_jgf, tiny_params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }

    void *h = static_cast<void *> (rq);

    // Allocate a job
    uint64_t jobid1 = 1;
    bool reserved = false;
    std::string R1;
    int64_t at = 0;
    double ov = 0.0;

    int rc = reapi_cli_t::match_allocate (h,
                                          MATCH_ALLOCATE,
                                          simple_jobspec,
                                          jobid1,
                                          reserved,
                                          R1,
                                          at,
                                          ov);
    if (rc < 0) {
        delete rq;
        BAIL_OUT ("allocation failed");
    }

    // First clone
    resource_query_t *clone1 = nullptr;
    try {
        clone1 = new resource_query_t (*rq);
    } catch (...) {
        delete rq;
        BAIL_OUT ("first clone failed");
    }

    // Clone the clone
    resource_query_t *clone2 = nullptr;
    try {
        clone2 = new resource_query_t (*clone1);
    } catch (...) {
        delete clone1;
        delete rq;
        BAIL_OUT ("second clone failed");
    }
    ok (true, "clone of clone succeeded");

    void *h_clone2 = static_cast<void *> (clone2);

    // Verify allocation is in clone2
    std::string mode;
    int64_t at_out;
    double ov_out;
    rc = reapi_cli_t::info (h_clone2, jobid1, mode, reserved, at_out, ov_out);
    ok (rc == 0, "allocation present in clone-of-clone");

    // Cancel in clone2, verify independence
    rc = reapi_cli_t::cancel (h_clone2, jobid1, false);
    ok (rc == 0, "cancel in clone2 succeeded");

    void *h_clone1 = static_cast<void *> (clone1);
    rc = reapi_cli_t::info (h_clone1, jobid1, mode, reserved, at_out, ov_out);
    ok (rc == 0, "allocation still in clone1");

    rc = reapi_cli_t::info (h, jobid1, mode, reserved, at_out, ov_out);
    ok (rc == 0, "allocation still in original");

    delete clone2;
    delete clone1;
    delete rq;
    return 0;
}

static int test_clone_metadata_structures ()
{
    // Setup: create context
    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (two_node_jgf, tiny_params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }

    // Clone
    resource_query_t *clone = nullptr;
    try {
        clone = new resource_query_t (*rq);
    } catch (...) {
        delete rq;
        BAIL_OUT ("clone failed");
    }

    // Verify metadata structures were copied
    ok (clone->db->metadata.by_type.size () > 0, "by_type map copied");
    ok (clone->db->metadata.by_path.size () > 0, "by_path map copied");
    ok (clone->db->metadata.by_outedges.size () > 0, "by_outedges map copied");
    ok (clone->db->metadata.roots.size () > 0, "roots map copied");

    // Verify node count is correct (should have 2 nodes)
    auto by_type_iter = clone->db->metadata.by_type.find (node_rt);
    ok (by_type_iter != clone->db->metadata.by_type.end (), "node type exists in by_type");
    if (by_type_iter != clone->db->metadata.by_type.end ()) {
        ok (by_type_iter->second.size () == 2, "by_type has 2 nodes");
    }

    delete clone;
    delete rq;
    return 0;
}

static int test_clone_error_message_independence ()
{
    // Setup: create context and clone
    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (tiny_jgf, tiny_params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }

    resource_query_t *clone = nullptr;
    try {
        clone = new resource_query_t (*rq);
    } catch (...) {
        delete rq;
        BAIL_OUT ("clone failed");
    }

    // Cause an error in clone (cancel non-existent job)
    void *h_clone = static_cast<void *> (clone);
    errno = 0;
    int rc = reapi_cli_t::cancel (h_clone, 999, false);
    ok (rc == -1 && errno == ENOENT, "cancel non-existent job in clone fails with ENOENT");

    std::string clone_err = clone->get_resource_query_err_msg ();
    std::string orig_err = rq->get_resource_query_err_msg ();

    // Verify error message independence
    ok (clone_err != orig_err || clone_err.empty (),
        "clone and original have independent error messages");

    // Clear clone error and verify original unaffected
    clone->clear_resource_query_err_msg ();
    std::string orig_err_after = rq->get_resource_query_err_msg ();
    ok (orig_err == orig_err_after, "clearing clone error doesn't affect original");

    delete clone;
    delete rq;
    return 0;
}

static int test_clone_planner_state ()
{
    // Setup: create context with two nodes
    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (two_node_jgf, tiny_params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }

    void *h = static_cast<void *> (rq);

    // Allocate both nodes with different durations to create different free times
    // Node 0: duration 60, will be free at t=60
    // Node 1: duration 120, will be free at t=120
    uint64_t jobid_alloc1 = 1;
    uint64_t jobid_alloc2 = 2;
    bool reserved = false;
    std::string R_alloc;
    int64_t at = 0;
    double ov = 0.0;

    int rc = reapi_cli_t::match_allocate (h,
                                          MATCH_ALLOCATE,
                                          short_jobspec,
                                          jobid_alloc1,
                                          reserved,
                                          R_alloc,
                                          at,
                                          ov);
    if (rc < 0) {
        delete rq;
        BAIL_OUT ("first allocation failed");
    }

    rc = reapi_cli_t::match_allocate (h,
                                      MATCH_ALLOCATE,
                                      long_jobspec,
                                      jobid_alloc2,
                                      reserved,
                                      R_alloc,
                                      at,
                                      ov);
    if (rc < 0) {
        delete rq;
        BAIL_OUT ("second allocation failed");
    }

    // Now make reservations (resources are busy, so these will be reserved)
    uint64_t jobid1 = 3;
    std::string R1;
    int64_t at1 = 100;
    rc = reapi_cli_t::match_allocate (h,
                                      MATCH_ALLOCATE_ORELSE_RESERVE,
                                      simple_jobspec,
                                      jobid1,
                                      reserved,
                                      R1,
                                      at1,
                                      ov);
    if (rc < 0 || !reserved) {
        delete rq;
        BAIL_OUT ("first reservation failed");
    }

    // Make another reservation at later time
    uint64_t jobid2 = 4;
    int64_t at2 = 200;
    std::string R2;
    rc = reapi_cli_t::match_allocate (h,
                                      MATCH_ALLOCATE_ORELSE_RESERVE,
                                      simple_jobspec,
                                      jobid2,
                                      reserved,
                                      R2,
                                      at2,
                                      ov);
    if (rc < 0 || !reserved) {
        delete rq;
        BAIL_OUT ("second reservation failed");
    }
    ok (at1 != at2, "two reservations scheduled at different times");

    // Clone
    resource_query_t *clone = nullptr;
    try {
        clone = new resource_query_t (*rq);
    } catch (...) {
        delete rq;
        BAIL_OUT ("clone failed");
    }

    void *h_clone = static_cast<void *> (clone);

    // Verify both reservations are in clone with correct times
    std::string mode;
    int64_t at_out;
    double ov_out;
    rc = reapi_cli_t::info (h_clone, jobid1, mode, reserved, at_out, ov_out);
    ok (rc == 0 && at_out == at1, "reservation 1 copied with correct time");

    rc = reapi_cli_t::info (h_clone, jobid2, mode, reserved, at_out, ov_out);
    ok (rc == 0 && at_out == at2, "reservation 2 copied with correct time");

    // Cancel one reservation in clone, verify original unaffected
    rc = reapi_cli_t::cancel (h_clone, jobid1, false);
    ok (rc == 0, "cancel reservation 1 in clone succeeded");

    rc = reapi_cli_t::info (h, jobid1, mode, reserved, at_out, ov_out);
    ok (rc == 0, "reservation 1 still in original");

    delete clone;
    delete rq;
    return 0;
}

static int test_clone_perf_stats_independence ()
{
    // Setup: create context
    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (two_node_jgf, tiny_params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }

    void *h = static_cast<void *> (rq);

    // Do some match operations to build up perf stats
    uint64_t jobid1 = 1;
    bool reserved = false;
    std::string R1;
    int64_t at = 0;
    double ov = 0.0;

    int rc = reapi_cli_t::match_allocate (h,
                                          MATCH_ALLOCATE,
                                          simple_jobspec,
                                          jobid1,
                                          reserved,
                                          R1,
                                          at,
                                          ov);
    if (rc < 0) {
        delete rq;
        BAIL_OUT ("allocation failed");
    }

    // Capture original perf stats (may be zero if not yet implemented)
    double orig_min = rq->perf.min;
    double orig_max = rq->perf.max;
    double orig_accum = rq->perf.accum;

    // Clone
    resource_query_t *clone = nullptr;
    try {
        clone = new resource_query_t (*rq);
    } catch (...) {
        delete rq;
        BAIL_OUT ("clone failed");
    }

    // Verify perf stats were copied
    ok (clone->perf.min == orig_min && clone->perf.max == orig_max
            && clone->perf.accum == orig_accum,
        "clone has same initial perf stats as original");

    // Modify clone's perf stats directly to verify independence
    clone->perf.accum = orig_accum + 1.0;
    clone->perf.min = orig_min + 1.0;
    clone->perf.max = orig_max + 1.0;

    // Verify original is unaffected
    ok (rq->perf.accum == orig_accum && rq->perf.min == orig_min && rq->perf.max == orig_max,
        "original perf stats unchanged after modifying clone");

    delete clone;
    delete rq;
    return 0;
}

static int test_clone_traverser_independence ()
{
    // Setup: create context
    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (two_node_jgf, tiny_params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }

    // Clone
    resource_query_t *clone = nullptr;
    try {
        clone = new resource_query_t (*rq);
    } catch (...) {
        delete rq;
        BAIL_OUT ("clone failed");
    }

    void *h_orig = static_cast<void *> (rq);
    void *h_clone = static_cast<void *> (clone);

    // Verify traverser works on both (do concurrent allocations)
    uint64_t jobid1 = 1;
    uint64_t jobid2 = 2;
    bool reserved = false;
    std::string R1, R2;
    int64_t at = 0;
    double ov = 0.0;

    int rc1 = reapi_cli_t::match_allocate (h_orig,
                                           MATCH_ALLOCATE,
                                           simple_jobspec,
                                           jobid1,
                                           reserved,
                                           R1,
                                           at,
                                           ov);
    int rc2 = reapi_cli_t::match_allocate (h_clone,
                                           MATCH_ALLOCATE,
                                           simple_jobspec,
                                           jobid2,
                                           reserved,
                                           R2,
                                           at,
                                           ov);

    ok (rc1 == 0 && rc2 == 0, "traverser works independently on both contexts");

    // Do another allocation on each to verify traverser state is truly independent
    uint64_t jobid3 = 3;
    uint64_t jobid4 = 4;
    std::string R3, R4;

    rc1 = reapi_cli_t::match_allocate (h_orig,
                                       MATCH_ALLOCATE,
                                       simple_jobspec,
                                       jobid3,
                                       reserved,
                                       R3,
                                       at,
                                       ov);
    rc2 = reapi_cli_t::match_allocate (h_clone,
                                       MATCH_ALLOCATE,
                                       simple_jobspec,
                                       jobid4,
                                       reserved,
                                       R4,
                                       at,
                                       ov);

    ok (rc1 == 0 && rc2 == 0, "multiple traverser operations work independently on both contexts");

    delete clone;
    delete rq;
    return 0;
}

int main (int argc, char *argv[])
{
    plan (NO_PLAN);

    test_clone_basic ();
    test_clone_independence ();
    test_clone_copies_allocations ();
    test_clone_copies_reservations ();
    test_clone_copies_up_down_status ();
    test_clone_with_multiple_jobs ();
    test_clone_of_clone ();
    test_clone_metadata_structures ();
    test_clone_error_message_independence ();
    test_clone_planner_state ();
    test_clone_perf_stats_independence ();
    test_clone_traverser_independence ();

    done_testing ();
    return EXIT_SUCCESS;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
