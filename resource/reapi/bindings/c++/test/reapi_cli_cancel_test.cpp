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
#include <jansson.h>
#include "resource/reapi/bindings/c++/reapi_cli.hpp"
#include "resource/policies/base/match_op.h"
#include "src/common/libtap/tap.h"

using namespace Flux::resource_model;
using namespace Flux::resource_model::detail;

static const char *tiny_jgf = R"({
    "graph": {
        "nodes": [
            {"id": "0", "metadata": {"type": "cluster", "basename": "tiny", "name": "tiny0", "size": 1, "paths": {"containment": "/tiny0"}}},
            {"id": "1", "metadata": {"type": "node", "basename": "node", "name": "node0", "size": 1, "rank": 0, "paths": {"containment": "/tiny0/node0"}}},
            {"id": "2", "metadata": {"type": "core", "basename": "core", "name": "core0", "size": 1, "id": 0, "rank": 0, "paths": {"containment": "/tiny0/node0/core0"}}},
            {"id": "3", "metadata": {"type": "core", "basename": "core", "name": "core1", "size": 1, "id": 1, "rank": 0, "paths": {"containment": "/tiny0/node0/core1"}}}
        ],
        "edges": [
            {"source": "0", "target": "1"},
            {"source": "1", "target": "2"},
            {"source": "1", "target": "3"}
        ]
    }
})";

static const char *two_core_jobspec = R"({
    "version": 1,
    "resources": [
        {
            "type": "node",
            "count": 1,
            "with": [
                {
                    "type": "slot",
                    "count": 2,
                    "label": "task",
                    "with": [{"type": "core", "count": 1}]
                }
            ]
        }
    ],
    "tasks": [{"command": ["sleep", "0"], "slot": "task", "count": {"per_slot": 1}}],
    "attributes": {"system": {"duration": 60.0}}
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

static const char *two_node_jobspec = R"({
    "version": 1,
    "resources": [
        {
            "type": "node",
            "count": 2,
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

static int test_cancel_full ()
{
    // Test full cancel using simple 3-parameter overload
    std::string params =
        "{\"load_format\": \"jgf\", \"matcher_policy\": \"high\", "
        "\"match_format\": \"rv1\", \"matcher_name\": \"CA\"}";

    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (tiny_jgf, params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }

    void *h = static_cast<void *> (rq);
    uint64_t jobid = 1;
    bool reserved = false;
    std::string R;
    int64_t at = 0;
    double ov = 0.0;

    // Allocate a job (precondition for the cancel under test)
    int rc = reapi_cli_t::match_allocate (h,
                                          MATCH_ALLOCATE,
                                          two_core_jobspec,
                                          jobid,
                                          reserved,
                                          R,
                                          at,
                                          ov);

    if (rc != 0)
        BAIL_OUT ("match_allocate failed to set up full cancel test");

    // Test full cancel using the simple 3-parameter cancel overload
    errno = 0;
    rc = reapi_cli_t::cancel (h, jobid, false);
    ok (rc == 0, "cancel (3-parameter) succeeds for full cancel");

    delete rq;
    return 0;
}

static int test_cancel_with_r ()
{
    // Test 4-parameter cancel with full R (auto-detects format from R)
    // Load graph from JGF and emit matches in rv1 format
    //
    // TODO: This test currently fails because JGF partial_cancel doesn't work
    // with the R returned from match_allocate. The scheduling.graph contains
    // resource structure but not allocation span metadata (which job owns which
    // resources). JGF partial_cancel expects to find allocation spans in the
    // input graph, but they only exist in the main resource graph managed by
    // the scheduler. This needs to be fixed before JGF format detection can work.
    std::string params =
        "{\"load_format\": \"jgf\", \"matcher_policy\": \"high\", "
        "\"match_format\": \"rv1\", \"matcher_name\": \"CA\"}";

    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (tiny_jgf, params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }

    void *h = static_cast<void *> (rq);
    uint64_t jobid = 10;
    bool reserved = false;
    std::string R;
    int64_t at = 0;
    double ov = 0.0;

    // Allocate a job (precondition for the cancel under test)
    int rc = reapi_cli_t::match_allocate (h,
                                          MATCH_ALLOCATE,
                                          two_core_jobspec,
                                          jobid,
                                          reserved,
                                          R,
                                          at,
                                          ov);
    if (rc != 0)
        BAIL_OUT ("match_allocate failed to set up full R cancel test");

    // Cancel with 4-parameter overload (auto-detects format from R)
    bool full_removal = false;
    errno = 0;
    rc = reapi_cli_t::cancel (h, jobid, R, false, full_removal);
    todo ("JGF partial_cancel not yet supported with R from match_allocate");
    ok (rc == 0, "cancel (4-parameter) with full R succeeds");
    ok (full_removal == true, "cancel with full R reports full_removal=true");
    end_todo;

    // Verify job state is CANCELED
    std::string mode;
    bool reserved_out;
    int64_t at_out;
    double ov_out;
    rc = reapi_cli_t::info (h, jobid, mode, reserved_out, at_out, ov_out);
    ok (rc == 0, "job exists in jobs table");
    todo ("JGF partial_cancel not yet supported with R from match_allocate");
    ok (mode == "CANCELED", "job state is CANCELED");
    end_todo;

    // Verify allocation no longer exists
    errno = 0;
    rc = reapi_cli_t::cancel (h, jobid, false);
    todo ("JGF partial_cancel not yet supported with R from match_allocate");
    ok (rc == -1 && errno == ENOENT, "allocation no longer exists");
    end_todo;

    delete rq;
    return 0;
}

static int test_cancel_partial ()
{
    // Test partial cancel - allocate 2 nodes, release 1, verify partial removal
    // Note: partial cancel only works at node granularity with rv1exec format
    std::string params =
        "{\"load_format\": \"jgf\", \"matcher_policy\": \"high\", "
        "\"match_format\": \"rv1\", \"matcher_name\": \"CA\"}";

    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (two_node_jgf, params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }

    void *h = static_cast<void *> (rq);
    uint64_t jobid = 2;
    bool reserved = false;
    std::string R;
    int64_t at = 0;
    double ov = 0.0;

    // Allocate 2 nodes (precondition for the partial cancel under test)
    int rc = reapi_cli_t::match_allocate (h,
                                          MATCH_ALLOCATE,
                                          two_node_jobspec,
                                          jobid,
                                          reserved,
                                          R,
                                          at,
                                          ov);
    if (rc != 0)
        BAIL_OUT ("match_allocate failed to allocate 2 nodes");
    ok (rc == 0, "allocated 2 nodes");

    // Parse the R to extract just one node for partial cancel
    // R is in rv1 format with R_lite array containing 2 nodes
    json_error_t error;
    json_t *r_obj = json_loads (R.c_str (), 0, &error);
    if (!r_obj)
        BAIL_OUT ("couldn't parse R as JSON: %s", error.text);

    // Create a valid rv1 with just one node by modifying the rank idset
    // R_lite compresses homogeneous nodes, so it has one entry with rank="0-1"
    json_t *exec = json_object_get (r_obj, "execution");
    json_t *r_lite = json_object_get (exec, "R_lite");
    if (!r_lite || !json_is_array (r_lite)) {
        json_decref (r_obj);
        BAIL_OUT ("R missing execution.R_lite array");
    }

    size_t r_lite_size = json_array_size (r_lite);
    if (r_lite_size < 1) {
        json_decref (r_obj);
        BAIL_OUT ("R_lite is empty");
    }

    // Get the first (and likely only) entry and change rank from "0-1" to just "0"
    json_t *node_entry = json_array_get (r_lite, 0);
    json_object_set_new (node_entry, "rank", json_string ("0"));

    // Remove scheduling section for partial cancel - only R_lite matters
    // Per RFC 20, R without scheduling key uses rv1exec format
    json_object_del (r_obj, "scheduling");

    char *partial_r = json_dumps (r_obj, JSON_COMPACT);
    json_decref (r_obj);

    if (!partial_r)
        BAIL_OUT ("couldn't serialize partial R");

    // Test partial cancel with 4-parameter overload (auto-detects format from R)
    // R without scheduling key should be detected as rv1exec format
    bool full_removal = false;
    errno = 0;
    rc = reapi_cli_t::cancel (h, jobid, partial_r, false, full_removal);
    ok (rc == 0, "cancel (4-param) auto-detects rv1exec format from R");
    ok (full_removal == false, "partial cancel reports full_removal=false");

    // Verify we can't cancel the same resources again
    errno = 0;
    bool full_removal_dup = false;
    rc = reapi_cli_t::cancel (h, jobid, partial_r, false, full_removal_dup);
    ok (rc == -1, "second cancel of same partial R fails");
    free (partial_r);

    // Verify job still exists after partial cancel and is still allocated
    std::string mode;
    bool reserved_out;
    int64_t at_out;
    double ov_out;
    rc = reapi_cli_t::info (h, jobid, mode, reserved_out, at_out, ov_out);
    ok (rc == 0, "job still exists after partial cancel");
    ok (mode == "ALLOCATED", "job state is still ALLOCATED after partial cancel");

    // Now cancel the remainder (rank 1)
    // Create R with just rank 1
    json_t *r_obj2 = json_loads (R.c_str (), 0, &error);
    json_t *exec2 = json_object_get (r_obj2, "execution");
    json_t *r_lite2 = json_object_get (exec2, "R_lite");
    json_t *node_entry2 = json_array_get (r_lite2, 0);
    json_object_set_new (node_entry2, "rank", json_string ("1"));

    // Remove scheduling section for partial cancel
    json_object_del (r_obj2, "scheduling");

    char *remainder_r = json_dumps (r_obj2, JSON_COMPACT);
    json_decref (r_obj2);

    bool full_removal2 = false;
    errno = 0;
    rc = reapi_cli_t::cancel (h, jobid, remainder_r, false, full_removal2);
    ok (rc == 0, "cancel remainder succeeds");
    ok (full_removal2 == true, "canceling remainder reports full_removal=true");

    free (remainder_r);

    // Verify job state is now CANCELED after full removal
    rc = reapi_cli_t::info (h, jobid, mode, reserved_out, at_out, ov_out);
    ok (rc == 0, "job still exists in jobs table after full removal");
    ok (mode == "CANCELED", "job state is CANCELED after full removal");

    // Verify allocation no longer exists (should get ENOENT on further cancel attempts)
    errno = 0;
    rc = reapi_cli_t::cancel (h, jobid, false);
    ok (rc == -1 && errno == ENOENT, "allocation no longer exists after canceling all resources");

    delete rq;
    return 0;
}

static int test_cancel_nonexistent_job ()
{
    // Test cancel with format error handling
    std::string params =
        "{\"load_format\": \"jgf\", \"matcher_policy\": \"high\", "
        "\"match_format\": \"rv1\", \"matcher_name\": \"CA\"}";

    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (tiny_jgf, params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }

    void *h = static_cast<void *> (rq);
    uint64_t nonexistent_jobid = 99999;

    // Test with nonexistent job and noent_ok=false (using simple overload)
    errno = 0;
    int rc = reapi_cli_t::cancel (h, nonexistent_jobid, false);
    ok (rc == -1 && errno == ENOENT, "cancel returns -1 with errno=ENOENT for nonexistent job");

    // Test with nonexistent job and noent_ok=true
    errno = 0;
    rc = reapi_cli_t::cancel (h, nonexistent_jobid, true);
    ok (rc == 0, "cancel succeeds with noent_ok=true for nonexistent job");

    delete rq;
    return 0;
}

static int test_cancel_bad_writer ()
{
    // Format detection extracts the reader format from scheduling.writer per
    // RFC 20/40.  A writer URI that resolves to an unknown reader format should
    // fail cleanly with EINVAL when the reader cannot be created, rather than
    // proceeding with an invalid reader.
    std::string params =
        "{\"load_format\": \"jgf\", \"matcher_policy\": \"high\", "
        "\"match_format\": \"rv1\", \"matcher_name\": \"CA\"}";

    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (tiny_jgf, params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }

    void *h = static_cast<void *> (rq);
    uint64_t jobid = 1;
    bool reserved = false;
    std::string R;
    int64_t at = 0;
    double ov = 0.0;

    // Allocate a job so cancel reaches format detection in remove_job.
    // (cancel checks allocation_exists() first and returns ENOENT otherwise.)
    // This is a precondition for the test, not the behavior under test, so a
    // failure here means the test itself is broken -> bail out loudly.
    int rc = reapi_cli_t::match_allocate (h,
                                          MATCH_ALLOCATE,
                                          two_core_jobspec,
                                          jobid,
                                          reserved,
                                          R,
                                          at,
                                          ov);
    if (rc != 0)
        BAIL_OUT ("match_allocate failed to set up bad-writer test");

    bool full_removal = false;

    // "fluxion:notaformat" -> strips "fluxion:" prefix -> format "notaformat",
    // which is not a known reader.
    std::string r_bad_fluxion =
        R"({"version": 1, "execution": {"R_lite": [{"rank": "0", "children": {"core": "0"}}]}, "scheduling": {"writer": "fluxion:notaformat"}})";
    errno = 0;
    rc = reapi_cli_t::cancel (h, jobid, r_bad_fluxion, false, full_removal);
    ok (rc == -1 && errno == EINVAL,
        "cancel with scheduling.writer=fluxion:notaformat fails with EINVAL");

    // "notfluxion:somethingelse" -> not a fluxion writer -> format used verbatim,
    // which is not a known reader.
    std::string r_bad_writer =
        R"({"version": 1, "execution": {"R_lite": [{"rank": "0", "children": {"core": "0"}}]}, "scheduling": {"writer": "notfluxion:somethingelse"}})";
    errno = 0;
    rc = reapi_cli_t::cancel (h, jobid, r_bad_writer, false, full_removal);
    ok (rc == -1 && errno == EINVAL,
        "cancel with scheduling.writer=notfluxion:somethingelse fails with EINVAL");

    delete rq;
    return 0;
}

static int test_cancel_null_ctx ()
{
    // Test cancel with NULL context
    uint64_t jobid = 1;
    bool full_removal = false;
    std::string R = "{}";

    errno = 0;
    int rc = reapi_cli_t::cancel (nullptr, jobid, R, false, full_removal);

    ok (rc == -1 && errno == EINVAL, "cancel returns -1 with errno=EINVAL for NULL context");

    return 0;
}

int main (int argc, char *argv[])
{
    plan (NO_PLAN);

    test_cancel_full ();
    test_cancel_with_r ();
    test_cancel_partial ();
    test_cancel_nonexistent_job ();
    test_cancel_bad_writer ();
    test_cancel_null_ctx ();

    done_testing ();
    return EXIT_SUCCESS;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
