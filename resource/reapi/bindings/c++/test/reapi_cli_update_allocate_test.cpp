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

static int test_update_allocate_basic ()
{
    std::string jgf = R"({
        "graph": {
            "nodes": [
                {"id": "0", "metadata": {"type": "cluster", "basename": "tiny", "name": "tiny0", "size": 1, "paths": {"containment": "/tiny0"}}},
                {"id": "1", "metadata": {"type": "node", "basename": "node", "name": "node0", "size": 1, "rank": 0, "paths": {"containment": "/tiny0/node0"}}},
                {"id": "2", "metadata": {"type": "core", "basename": "core", "name": "core0", "size": 1, "id": 0, "rank": 0, "paths": {"containment": "/tiny0/node0/core0"}}}
            ],
            "edges": [{"source": "0", "target": "1"}, {"source": "1", "target": "2"}]
        }
    })";

    std::string params =
        "{\"load_format\": \"jgf\", \"matcher_policy\": \"high\", "
        "\"match_format\": \"rv1\", \"matcher_name\": \"CA\"}";

    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (jgf, params);
    } catch (...) {
        ok (1, "# SKIP: couldn't create resource_query_t");
        return 0;
    }

    void *h = static_cast<void *> (rq);
    uint64_t jobid = 1;
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
        "tasks": [{"command": ["sleep", "0"], "slot": "task", "count": {"per_slot": 1}}],
        "attributes": {"system": {"duration": 60.0}}
    })";

    // Allocate a job to get an R string
    errno = 0;
    int rc = reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, jobspec, jobid, reserved, R, at, ov);
    if (rc != 0)
        BAIL_OUT ("match_allocate failed for jobid 1");

    ok (rc == 0, "match_allocate succeeded for jobid 1");

    // Cancel the job to free resources
    rc = reapi_cli_t::cancel (h, jobid, false);
    ok (rc == 0, "cancel succeeded for jobid 1");

    // Test update_allocate with a different jobid
    uint64_t jobid2 = 2;
    std::string R_out;
    int64_t at_out = 0;
    double ov_out = 0.0;

    errno = 0;
    rc = reapi_cli_t::update_allocate (h, jobid2, R, at_out, ov_out, R_out);
    ok (rc == 0, "update_allocate succeeded for jobid 2");
    ok (!R_out.empty (), "update_allocate returned non-empty R_out");

    delete rq;
    return 0;
}

static int test_update_allocate_duplicate_jobid ()
{
    std::string jgf = R"({
        "graph": {
            "nodes": [
                {"id": "0", "metadata": {"type": "cluster", "basename": "tiny", "name": "tiny0", "size": 1, "paths": {"containment": "/tiny0"}}},
                {"id": "1", "metadata": {"type": "node", "basename": "node", "name": "node0", "size": 1, "rank": 0, "paths": {"containment": "/tiny0/node0"}}},
                {"id": "2", "metadata": {"type": "core", "basename": "core", "name": "core0", "size": 1, "id": 0, "rank": 0, "paths": {"containment": "/tiny0/node0/core0"}}}
            ],
            "edges": [{"source": "0", "target": "1"}, {"source": "1", "target": "2"}]
        }
    })";

    std::string params =
        "{\"load_format\": \"jgf\", \"matcher_policy\": \"high\", "
        "\"match_format\": \"rv1\", \"matcher_name\": \"CA\"}";

    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (jgf, params);
    } catch (...) {
        ok (1, "# SKIP: couldn't create resource_query_t");
        return 0;
    }

    void *h = static_cast<void *> (rq);
    uint64_t jobid = 1;
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
        "tasks": [{"command": ["sleep", "0"], "slot": "task", "count": {"per_slot": 1}}],
        "attributes": {"system": {"duration": 60.0}}
    })";

    // Allocate a job
    errno = 0;
    int rc = reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, jobspec, jobid, reserved, R, at, ov);
    if (rc != 0)
        BAIL_OUT ("match_allocate failed for jobid 1");

    // Try update_allocate with the same jobid - should fail with EEXIST
    std::string R_out;
    int64_t at_out = 0;
    double ov_out = 0.0;

    errno = 0;
    rc = reapi_cli_t::update_allocate (h, jobid, R, at_out, ov_out, R_out);
    ok (rc == -1 && errno == EEXIST,
        "update_allocate returns -1 with errno=EEXIST for duplicate jobid");

    delete rq;
    return 0;
}

static int test_update_allocate_null_ctx ()
{
    void *h = nullptr;
    uint64_t jobid = 1;
    std::string R = "{}";
    std::string R_out;
    int64_t at = 0;
    double ov = 0.0;

    errno = 0;
    int rc = reapi_cli_t::update_allocate (h, jobid, R, at, ov, R_out);

    ok (rc == -1 && errno == EINVAL,
        "update_allocate returns -1 with errno=EINVAL for NULL context");

    return 0;
}

int main (int argc, char *argv[])
{
    plan (NO_PLAN);

    test_update_allocate_basic ();
    test_update_allocate_duplicate_jobid ();
    test_update_allocate_null_ctx ();

    done_testing ();
    return EXIT_SUCCESS;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
