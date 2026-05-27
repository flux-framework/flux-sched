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
#include <jansson.h>
}

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include "resource/reapi/bindings/c++/reapi_cli.hpp"
#include "src/common/libtap/tap.h"

using namespace Flux::resource_model;
using namespace Flux::resource_model::detail;

static void diag_json (json_t *obj)
{
    char *str = json_dumps (obj, JSON_COMPACT);
    if (str) {
        diag ("%s", str);
        free (str);
    }
}

static const char *two_node_jgf = R"({
  "version": 1,
  "graph": {
    "nodes": [
      {"id": "0", "metadata": {"type": "cluster", "basename": "cluster", "name": "cluster0", "paths": {"containment": "/cluster0"}}},
      {"id": "1", "metadata": {"type": "node", "basename": "node", "name": "node0", "id": 0, "uniq_id": 0, "rank": 0, "paths": {"containment": "/cluster0/node0"}}},
      {"id": "2", "metadata": {"type": "core", "basename": "core", "name": "core0", "id": 0, "uniq_id": 0, "paths": {"containment": "/cluster0/node0/core0"}}},
      {"id": "3", "metadata": {"type": "core", "basename": "core", "name": "core1", "id": 1, "uniq_id": 1, "paths": {"containment": "/cluster0/node0/core1"}}},
      {"id": "4", "metadata": {"type": "node", "basename": "node", "name": "node1", "id": 1, "uniq_id": 1, "rank": 1, "paths": {"containment": "/cluster0/node1"}}},
      {"id": "5", "metadata": {"type": "core", "basename": "core", "name": "core0", "id": 0, "uniq_id": 2, "paths": {"containment": "/cluster0/node1/core0"}}},
      {"id": "6", "metadata": {"type": "core", "basename": "core", "name": "core1", "id": 1, "uniq_id": 3, "paths": {"containment": "/cluster0/node1/core1"}}}
    ],
    "edges": [
      {"source": "0", "target": "1", "metadata": {"name": {"containment": "contains"}}},
      {"source": "1", "target": "2", "metadata": {"name": {"containment": "contains"}}},
      {"source": "1", "target": "3", "metadata": {"name": {"containment": "contains"}}},
      {"source": "0", "target": "4", "metadata": {"name": {"containment": "contains"}}},
      {"source": "4", "target": "5", "metadata": {"name": {"containment": "contains"}}},
      {"source": "4", "target": "6", "metadata": {"name": {"containment": "contains"}}}
    ]
  }
})";

static const char *params = R"({
    "load_format": "jgf",
    "matcher_policy": "high",
    "match_format": "rv1",
    "matcher_name": "CA"
})";

static bool is_up (const char *status)
{
    if (status && !strcmp (status, "UP"))
        return true;
    return false;
}
static bool is_down (const char *status)
{
    if (status && !strcmp (status, "DOWN"))
        return true;
    return false;
}

static void test_set_status_by_path ()
{
    resource_query_t *ctx = nullptr;
    const char *status;
    int rc;

    ctx = new resource_query_t (two_node_jgf, params);
    ok (ctx != nullptr, "set_status_by_path: resource_query_t created");

    // Test setting node to down
    rc = reapi_cli_t::set_status (ctx, "/cluster0/node0", "DOWN");
    ok (rc == 0, "set_status_by_path: set node0 to down succeeds");

    // Verify it's down
    rc = reapi_cli_t::get_status (ctx, "/cluster0/node0", status);
    ok (rc == 0 && is_down (status), "set_status_by_path: node0 status is down");

    // Test setting back to up
    rc = reapi_cli_t::set_status (ctx, "/cluster0/node0", "UP");
    ok (rc == 0, "set_status_by_path: set node0 to up succeeds");

    rc = reapi_cli_t::get_status (ctx, "/cluster0/node0", status);
    ok (rc == 0 && is_up (status), "set_status_by_path: node0 status is up");

    // Test invalid path
    errno = 0;
    rc = reapi_cli_t::set_status (ctx, "/cluster0/nonexistent", "DOWN");
    ok (rc == -1 && errno == EINVAL, "set_status_by_path: invalid path returns EINVAL");

    // Test empty path
    errno = 0;
    rc = reapi_cli_t::set_status (ctx, "", "DOWN");
    ok (rc == -1 && errno == EINVAL, "set_status_by_path: empty path returns EINVAL");

    delete ctx;
}

static void test_set_status_by_rank ()
{
    resource_query_t *ctx = nullptr;
    const char *status;
    int rc;

    ctx = new resource_query_t (two_node_jgf, params);
    ok (ctx != nullptr, "set_status_by_rank: resource_query_t created");

    // Test setting rank 0 to down
    rc = reapi_cli_t::set_status (ctx, 0, "DOWN");
    ok (rc == 0, "set_status_by_rank: set rank 0 to down succeeds");

    // Verify it's down
    rc = reapi_cli_t::get_status (ctx, 0, status);
    ok (rc == 0 && is_down (status), "set_status_by_rank: rank 0 status is down");

    // Test setting back to up
    rc = reapi_cli_t::set_status (ctx, 0, "UP");
    ok (rc == 0, "set_status_by_rank: set rank 0 to up succeeds");

    rc = reapi_cli_t::get_status (ctx, 0, status);
    ok (rc == 0 && is_up (status), "set_status_by_rank: rank 0 status is up");

    // Test invalid rank
    errno = 0;
    rc = reapi_cli_t::set_status (ctx, 999, "DOWN");
    ok (rc == -1 && errno == ENOENT, "set_status_by_rank: invalid rank returns ENOENT");

    delete ctx;
}

static void test_set_status_all_ranks ()
{
    resource_query_t *ctx = nullptr;
    const char *status;
    int rc;

    ctx = new resource_query_t (two_node_jgf, params);
    ok (ctx != nullptr, "set_status_all_ranks: resource_query_t created");

    // Set all ranks to down using FLUX_NODEID_ANY
    rc = reapi_cli_t::set_status (ctx, FLUX_NODEID_ANY, "DOWN");
    ok (rc == 0, "set_status_all_ranks: set all ranks to down succeeds");

    // Verify both ranks are down
    rc = reapi_cli_t::get_status (ctx, 0, status);
    ok (rc == 0 && is_down (status), "set_status_all_ranks: rank 0 is down");

    rc = reapi_cli_t::get_status (ctx, 1, status);
    ok (rc == 0 && is_down (status), "set_status_all_ranks: rank 1 is down");

    // Set all back to up
    rc = reapi_cli_t::set_status (ctx, FLUX_NODEID_ANY, "UP");
    ok (rc == 0, "set_status_all_ranks: set all ranks to up succeeds");

    rc = reapi_cli_t::get_status (ctx, 0, status);
    ok (rc == 0 && is_up (status), "set_status_all_ranks: rank 0 is up");

    rc = reapi_cli_t::get_status (ctx, 1, status);
    ok (rc == 0 && is_up (status), "set_status_all_ranks: rank 1 is up");

    delete ctx;
}

static void test_status_independence ()
{
    resource_query_t *ctx = nullptr;
    const char *status;
    int rc;

    ctx = new resource_query_t (two_node_jgf, params);
    ok (ctx != nullptr, "status_independence: resource_query_t created");

    // Set node0 to down - children remain independent
    rc = reapi_cli_t::set_status (ctx, "/cluster0/node0", "DOWN");
    ok (rc == 0, "status_independence: set node0 to down succeeds");

    // Cores are independent - not automatically affected by parent
    rc = reapi_cli_t::get_status (ctx, "/cluster0/node0/core0", status);
    ok (rc == 0 && is_up (status), "status_independence: core0 status is independent");

    rc = reapi_cli_t::get_status (ctx, "/cluster0/node0/core1", status);
    ok (rc == 0 && is_up (status), "status_independence: core1 status is independent");

    // Check that node1 is unaffected
    rc = reapi_cli_t::get_status (ctx, "/cluster0/node1", status);
    ok (rc == 0 && is_up (status), "status_independence: node1 is still up");

    delete ctx;
}

static void test_rank_status_node_level ()
{
    resource_query_t *ctx = nullptr;
    const char *status;
    int rc;

    ctx = new resource_query_t (two_node_jgf, params);
    ok (ctx != nullptr, "rank_status_node_level: resource_query_t created");

    // Initially all should be up
    rc = reapi_cli_t::get_status (ctx, 0, status);
    ok (rc == 0 && is_up (status), "rank_status_node_level: rank 0 starts as up");

    // Set the node at rank 0 to down
    rc = reapi_cli_t::set_status (ctx, "/cluster0/node0", "DOWN");
    ok (rc == 0, "rank_status_node_level: set node0 to down succeeds");

    // Rank query reflects node status
    rc = reapi_cli_t::get_status (ctx, 0, status);
    ok (rc == 0 && is_down (status), "rank_status_node_level: rank 0 is down when node is down");

    // Set the node back to up
    rc = reapi_cli_t::set_status (ctx, "/cluster0/node0", "UP");
    ok (rc == 0, "rank_status_node_level: set node back to up succeeds");

    // Rank should be up again
    rc = reapi_cli_t::get_status (ctx, 0, status);
    ok (rc == 0 && is_up (status), "rank_status_node_level: rank 0 is up when node is up");

    delete ctx;
}

static void test_get_status_errors ()
{
    resource_query_t *ctx = nullptr;
    const char *status;
    int rc;

    ctx = new resource_query_t (two_node_jgf, params);
    ok (ctx != nullptr, "get_status_errors: resource_query_t created");

    // Test get with invalid path
    errno = 0;
    rc = reapi_cli_t::get_status (ctx, "/cluster0/nonexistent", status);
    ok (rc == -1 && errno == ENOENT, "get_status_errors: invalid path returns ENOENT");

    // Test get with empty path
    errno = 0;
    rc = reapi_cli_t::get_status (ctx, "", status);
    ok (rc == -1 && errno == EINVAL, "get_status_errors: empty path returns EINVAL");

    // Test get with invalid rank
    errno = 0;
    rc = reapi_cli_t::get_status (ctx, 999, status);
    ok (rc == -1 && errno == ENOENT, "get_status_errors: invalid rank returns ENOENT");

    // Test get with FLUX_NODEID_ANY
    errno = 0;
    rc = reapi_cli_t::get_status (ctx, FLUX_NODEID_ANY, status);
    ok (rc == -1 && errno == EINVAL, "get_status_errors: FLUX_NODEID_ANY returns EINVAL");

    // Test get with null context
    errno = 0;
    rc = reapi_cli_t::get_status (nullptr, "/cluster0/node0", status);
    ok (rc == -1 && errno == EINVAL, "get_status_errors: null context returns EINVAL");

    delete ctx;
}

static void test_invalid_status_strings ()
{
    resource_query_t *ctx = nullptr;
    int rc;

    ctx = new resource_query_t (two_node_jgf, params);
    ok (ctx != nullptr, "invalid_status_strings: resource_query_t created");

    // Test set_status with invalid status string (by path)
    errno = 0;
    rc = reapi_cli_t::set_status (ctx, "/cluster0/node0", "INVALID");
    ok (rc == -1 && errno == EINVAL,
        "invalid_status_strings: set_status with invalid string returns EINVAL");

    // Test set_status with invalid status string (by rank)
    errno = 0;
    rc = reapi_cli_t::set_status (ctx, 0, "BOGUS");
    ok (rc == -1 && errno == EINVAL,
        "invalid_status_strings: set_status by rank with invalid string returns EINVAL");

    // Test with lowercase (should work - case insensitive)
    errno = 0;
    rc = reapi_cli_t::set_status (ctx, "/cluster0/node0", "down");
    ok (rc == 0, "invalid_status_strings: lowercase 'down' succeeds (case insensitive)");

    // Test with mixed case (should work - case insensitive)
    errno = 0;
    rc = reapi_cli_t::set_status (ctx, "/cluster0/node0", "Up");
    ok (rc == 0, "invalid_status_strings: mixed case 'Up' succeeds (case insensitive)");

    // Test with NULL status string (by path)
    errno = 0;
    rc = reapi_cli_t::set_status (ctx, "/cluster0/node0", nullptr);
    ok (rc == -1 && errno == EINVAL,
        "invalid_status_strings: set_status with NULL status returns EINVAL");

    // Test with NULL status string (by rank)
    errno = 0;
    rc = reapi_cli_t::set_status (ctx, 0, nullptr);
    ok (rc == -1 && errno == EINVAL,
        "invalid_status_strings: set_status by rank with NULL status returns EINVAL");

    delete ctx;
}

static void test_allocation_with_down_node ()
{
    resource_query_t *ctx = nullptr;
    int rc;

    try {
        ctx = new resource_query_t (two_node_jgf, params);
    } catch (...) {
        BAIL_OUT ("allocation_with_down_node: failed to create resource_query_t");
    }

    // Mark both nodes down
    rc = reapi_cli_t::set_status (ctx, 0, "DOWN");
    ok (rc == 0, "allocation_with_down_node: mark node0 down succeeds");

    rc = reapi_cli_t::set_status (ctx, 1, "DOWN");
    ok (rc == 0, "allocation_with_down_node: mark node1 down succeeds");

    // Try to allocate with all nodes down - should fail
    const char *one_core_jobspec = R"({
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
        "tasks": [{"command": ["sleep", "60"], "slot": "task", "count": {"per_slot": 1}}],
        "attributes": {"system": {"duration": 60}}
    })";

    std::string R;
    bool reserved = false;
    int64_t at = 0;
    double ov = 0.0;
    errno = 0;
    rc = reapi_cli_t::match_allocate (ctx,
                                      match_op_t::MATCH_ALLOCATE,
                                      one_core_jobspec,
                                      1,
                                      reserved,
                                      R,
                                      at,
                                      ov);

    ok (rc == -1, "allocation_with_down_node: allocation fails when all nodes are down");

    delete ctx;
}

static void test_allocation_with_mixed_status ()
{
    resource_query_t *ctx = nullptr;
    int rc;

    try {
        ctx = new resource_query_t (two_node_jgf, params);
    } catch (...) {
        BAIL_OUT ("allocation_with_mixed_status: failed to create resource_query_t");
    }

    // Mark rank 0 down, leave rank 1 up
    rc = reapi_cli_t::set_status (ctx, 0, "DOWN");
    ok (rc == 0, "allocation_with_mixed_status: mark rank 0 down succeeds");

    // Verify rank 1 is still up
    const char *status;
    rc = reapi_cli_t::get_status (ctx, 1, status);
    ok (rc == 0 && is_up (status), "allocation_with_mixed_status: rank 1 is up");

    // Try to allocate with one node down - should succeed on the up node
    const char *one_core_jobspec = R"({
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
        "tasks": [{"command": ["sleep", "60"], "slot": "task", "count": {"per_slot": 1}}],
        "attributes": {"system": {"duration": 60}}
    })";

    std::string R;
    bool reserved = false;
    int64_t at = 0;
    double ov = 0.0;
    errno = 0;
    rc = reapi_cli_t::match_allocate (ctx,
                                      match_op_t::MATCH_ALLOCATE,
                                      one_core_jobspec,
                                      1,
                                      reserved,
                                      R,
                                      at,
                                      ov);

    ok (rc == 0, "allocation_with_mixed_status: allocation succeeds with one node down");

    // Parse R to verify which rank was used
    json_error_t error;
    json_t *R_json = json_loads (R.c_str (), 0, &error);
    if (!R_json) {
        delete ctx;
        BAIL_OUT ("allocation_with_mixed_status: failed to parse R JSON: %s", error.text);
    }

    // Extract rank from R_lite[0].rank
    const char *rank_str = nullptr;
    if (json_unpack (R_json, "{s:{s:[{s:s}]}}", "execution", "R_lite", "rank", &rank_str) < 0) {
        diag ("R = ");
        diag_json (R_json);
        json_decref (R_json);
        delete ctx;
        BAIL_OUT ("allocation_with_mixed_status: failed to unpack rank from R");
    }

    if (strcmp (rank_str, "1") != 0) {
        diag ("R = ");
        diag_json (R_json);
    }
    ok (strcmp (rank_str, "1") == 0,
        "allocation_with_mixed_status: allocation used rank 1 (up node), not rank 0 (down node)");

    json_decref (R_json);
    delete ctx;
}

static void test_child_resource_status ()
{
    resource_query_t *ctx = nullptr;
    const char *status;
    int rc;

    ctx = new resource_query_t (two_node_jgf, params);
    ok (ctx != nullptr, "child_resource_status: resource_query_t created");

    // Set core0 on node0 to down, independent of node status
    rc = reapi_cli_t::set_status (ctx, "/cluster0/node0/core0", "DOWN");
    ok (rc == 0, "child_resource_status: set core0 to down succeeds");

    // Verify core0 is down
    rc = reapi_cli_t::get_status (ctx, "/cluster0/node0/core0", status);
    ok (rc == 0 && is_down (status), "child_resource_status: core0 status is down");

    // Verify node0 is still up (parent status is independent)
    rc = reapi_cli_t::get_status (ctx, "/cluster0/node0", status);
    ok (rc == 0 && is_up (status),
        "child_resource_status: node0 status is still up (independent of child)");

    // Verify core1 on node0 is still up (sibling status is independent)
    rc = reapi_cli_t::get_status (ctx, "/cluster0/node0/core1", status);
    ok (rc == 0 && is_up (status),
        "child_resource_status: core1 status is still up (independent of sibling)");

    // Set core0 back to up
    rc = reapi_cli_t::set_status (ctx, "/cluster0/node0/core0", "UP");
    ok (rc == 0, "child_resource_status: set core0 back to up succeeds");

    rc = reapi_cli_t::get_status (ctx, "/cluster0/node0/core0", status);
    ok (rc == 0 && is_up (status), "child_resource_status: core0 is back to up");

    delete ctx;
}

int main (int argc, char *argv[])
{
    plan (NO_PLAN);

    test_set_status_by_path ();
    test_set_status_by_rank ();
    test_set_status_all_ranks ();
    test_status_independence ();
    test_rank_status_node_level ();
    test_get_status_errors ();
    test_invalid_status_strings ();
    test_allocation_with_down_node ();
    test_allocation_with_mixed_status ();
    test_child_resource_status ();

    done_testing ();

    return EXIT_SUCCESS;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
