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

static int test_find_allocated ()
{
    std::string jgf = R"({
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

    // Use rv1_nosched format like resource broker module does
    std::string params =
        "{\"load_format\": \"jgf\", \"matcher_policy\": \"high\", "
        "\"match_format\": \"rv1_nosched\", \"matcher_name\": \"CA\"}";

    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (jgf, params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }

    void *h = static_cast<void *> (rq);
    bool reserved = false;
    std::string R;
    int64_t at = 0;
    double ov = 0.0;

    std::string jobspec = R"({
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
        "tasks": [{"command": ["app"], "slot": "task", "count": {"per_slot": 1}}],
        "attributes": {"system": {"duration": 60.0}}
    })";

    // Allocate two jobs
    uint64_t jobid1 = 1;
    int rc = reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, jobspec, jobid1, reserved, R, at, ov);
    if (rc != 0)
        BAIL_OUT ("match_allocate failed for jobid 1");
    ok (rc == 0, "allocated job 1");

    uint64_t jobid2 = 2;
    rc = reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, jobspec, jobid2, reserved, R, at, ov);
    if (rc != 0)
        BAIL_OUT ("match_allocate failed for jobid 2");
    ok (rc == 0, "allocated job 2");

    // Use find() to get all allocated resources
    // Pass explicit format string to test the optional parameter
    json_t *R_alloc = nullptr;
    errno = 0;
    rc = reapi_cli_t::find (h, "sched-now=allocated", R_alloc, "rv1_nosched");
    ok (rc == 0, "find sched-now=allocated succeeded");
    ok (R_alloc != nullptr, "find returned non-NULL R");

    // Verify we got valid JSON with execution key
    if (R_alloc) {
        ok (json_is_object (R_alloc), "R is a JSON object");
        json_t *execution = json_object_get (R_alloc, "execution");
        ok (execution != nullptr, "R has execution key");

        json_t *r_lite = json_object_get (execution, "R_lite");
        ok (r_lite != nullptr, "execution has R_lite");
        ok (json_is_array (r_lite), "R_lite is an array");

        // Should have entries for allocated resources
        size_t array_size = json_array_size (r_lite);
        ok (array_size > 0, "R_lite has entries for allocated resources");

        json_decref (R_alloc);
    }

    delete rq;
    return 0;
}

static int test_find_empty ()
{
    std::string jgf = R"({
        "graph": {
            "nodes": [
                {"id": "0", "metadata": {"type": "cluster", "basename": "tiny", "name": "tiny0", "size": 1, "paths": {"containment": "/tiny0"}}},
                {"id": "1", "metadata": {"type": "node", "basename": "node", "name": "node0", "size": 1, "rank": 0, "paths": {"containment": "/tiny0/node0"}}},
                {"id": "2", "metadata": {"type": "core", "basename": "core", "name": "core0", "size": 1, "id": 0, "rank": 0, "paths": {"containment": "/tiny0/node0/core0"}}}
            ],
            "edges": [
                {"source": "0", "target": "1"},
                {"source": "1", "target": "2"}
            ]
        }
    })";

    std::string params =
        "{\"load_format\": \"jgf\", \"matcher_policy\": \"high\", "
        "\"match_format\": \"rv1_nosched\", \"matcher_name\": \"CA\"}";

    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (jgf, params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }

    void *h = static_cast<void *> (rq);

    // Find allocated resources when none are allocated
    // Test with std::nullopt (default) to use context's format
    json_t *R_alloc = nullptr;
    errno = 0;
    int rc = reapi_cli_t::find (h, "sched-now=allocated", R_alloc, std::nullopt);
    ok (rc == 0, "find sched-now=allocated with no allocations succeeded");

    // When no resources match, emit_json returns NULL (expected behavior)
    ok (R_alloc == nullptr, "find returned NULL for empty result");

    delete rq;
    return 0;
}

static int test_find_null_ctx ()
{
    void *h = nullptr;
    json_t *o = nullptr;

    errno = 0;
    int rc = reapi_cli_t::find (h, "sched-now=allocated", o);

    ok (rc == -1 && errno == EINVAL, "find returns -1 with errno=EINVAL for NULL context");

    return 0;
}

int main (int argc, char *argv[])
{
    plan (NO_PLAN);

    test_find_allocated ();
    test_find_empty ();
    test_find_null_ctx ();

    done_testing ();
    return EXIT_SUCCESS;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
