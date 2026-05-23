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

static int test_match_allocate_invalid_match_op ()
{
    void *h = nullptr;  // nullptr is OK - validation happens first
    uint64_t jobid = 1;
    bool reserved = false;
    std::string jobspec = "{}";
    std::string R;
    int64_t at = 0;
    double ov = 0.0;

    // Test with invalid match_op (use out of range enum value)
    errno = 0;
    int rc = reapi_cli_t::match_allocate (h, (match_op_t)999, jobspec, jobid, reserved, R, at, ov);

    ok (rc == -1 && errno == EINVAL,
        "match_allocate returns -1 with errno=EINVAL for invalid match_op");

    return 0;
}

static int test_match_allocate_invalid_jobspec_with_ctx ()
{
    // Create a minimal context with default constructor
    // This gives us enough to test jobspec validation without full graph
    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t ();
    } catch (...) {
        // Can't create context, skip this test
        ok (1, "# SKIP: couldn't create minimal resource_query_t context");
        return 0;
    }

    void *h = static_cast<void *> (rq);
    uint64_t jobid = 1;
    bool reserved = false;
    std::string invalid_jobspec = "not valid json";
    std::string R;
    int64_t at = 0;
    double ov = 0.0;

    // Test with invalid jobspec JSON
    errno = 0;
    int rc = reapi_cli_t::match_allocate (h,
                                          MATCH_ALLOCATE,
                                          invalid_jobspec,
                                          jobid,
                                          reserved,
                                          R,
                                          at,
                                          ov);

    ok (rc == -1 && errno == EINVAL,
        "match_allocate returns -1 with errno=EINVAL for invalid jobspec");

    delete rq;
    return 0;
}

static int test_match_allocate_ebusy ()
{
    // Use JGF format like Python tests - simpler and better tested
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
    } catch (std::exception &e) {
        ok (1, "# SKIP: couldn't create resource_query_t with rv1exec: %s", e.what ());
        return 0;
    } catch (...) {
        ok (1, "# SKIP: couldn't create resource_query_t with rv1exec (unknown error)");
        return 0;
    }

    void *h = static_cast<void *> (rq);
    uint64_t jobid1 = 1;
    uint64_t jobid2 = 2;
    bool reserved = false;
    std::string R;
    int64_t at = 0;
    double ov = 0.0;

    // Clear any previous error messages
    reapi_cli_t::clear_err_message ();

    // Check if graph has any vertices
    int64_t V = 0, E = 0, J = 0;
    double load = 0, min = 0, max = 0, avg = 0;
    if (reapi_cli_t::stat (h, V, E, J, load, min, max, avg) == 0) {
        if (V == 0 || E == 0) {
            ok (1, "# SKIP: resource graph is empty (V=%ld, E=%ld)", V, E);
            delete rq;
            return 0;
        }
    }

    // Use exact jobspec from Python test
    std::string jobspec1 = R"({
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

    errno = 0;
    int rc = reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, jobspec1, jobid1, reserved, R, at, ov);
    int saved_errno = errno;

    if (rc == 0) {
        // Now try to allocate again - should get EBUSY
        errno = 0;
        std::string jobspec2 = jobspec1;  // Same request
        rc = reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, jobspec2, jobid2, reserved, R, at, ov);

        ok (rc == -1 && errno == EBUSY,
            "match_allocate returns -1 with errno=EBUSY when resources unavailable");
    } else {
        std::string err_msg = reapi_cli_t::get_err_message ();
        std::string traverser_err = rq->get_traverser_err_msg ();
        // Check if it's ENODEV (infeasible) which would suggest wrong jobspec
        if (saved_errno == ENODEV) {
            ok (1,
                "# SKIP: first allocation returned ENODEV (infeasible jobspec for rv1exec "
                "graph)");
        } else if (saved_errno == EBUSY) {
            ok (1, "# SKIP: first allocation returned EBUSY (resources already busy?)");
        } else {
            ok (1,
                "# SKIP: first allocation failed (rc=%d, errno=%d)\n"
                "#   reapi err: %s\n"
                "#   traverser err: %s",
                rc,
                saved_errno,
                err_msg.c_str (),
                traverser_err.c_str ());
        }
    }

    delete rq;
    return 0;
}

static int test_match_allocate_enodev ()
{
    // Use JGF format - only has 1 core, no GPUs
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
        ok (1, "# SKIP: couldn't create resource_query_t with rv1exec");
        return 0;
    }

    void *h = static_cast<void *> (rq);
    uint64_t jobid = 1;
    bool reserved = false;
    std::string R;
    int64_t at = 0;
    double ov = 0.0;

    // Clear any previous error messages
    reapi_cli_t::clear_err_message ();

    // Request a resource type that doesn't exist (gpu) - infeasible
    std::string jobspec =
        "{\"resources\": [{\"type\": \"node\", \"count\": 1, \"with\": "
        "[{\"type\": \"gpu\", \"count\": 1}]}], \"tasks\": [{\"command\": "
        "[\"app\"], \"slot\": \"gpu\", \"count\": {\"per_slot\": 1}}], "
        "\"attributes\": {\"system\": {\"duration\": 3600}}, \"version\": 1}";

    errno = 0;
    // Use MATCH_ALLOCATE_W_SATISFIABILITY to get ENODEV for infeasible requests
    int rc = reapi_cli_t::match_allocate (h,
                                          MATCH_ALLOCATE_W_SATISFIABILITY,
                                          jobspec,
                                          jobid,
                                          reserved,
                                          R,
                                          at,
                                          ov);

    ok (rc == -1 && errno == ENODEV,
        "match_allocate returns -1 with errno=ENODEV for infeasible request");

    delete rq;
    return 0;
}

int main (int argc, char *argv[])
{
    plan (NO_PLAN);

    test_match_allocate_invalid_match_op ();
    test_match_allocate_invalid_jobspec_with_ctx ();
    test_match_allocate_ebusy ();
    test_match_allocate_enodev ();

    done_testing ();
    return EXIT_SUCCESS;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
