/*****************************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

/* A job holds one core of a two-core node past its planned end. A request
 * for both cores at a later time must be refused as an allocation, but
 * reserved one second later, although the planners hold no scheduled point
 * after the planned end of the job. The match API of the CLI binding
 * matches at time 0 only, so the test runs the traverser at the later
 * time directly. The test reads the job index of the graph to show that
 * a reservation is recorded and a match without allocation is not.
 */

extern "C" {
#if HAVE_CONFIG_H
#include <config.h>
#endif
}

#include <cerrno>
#include <sstream>
#include <string>
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

static const char *one_core_jobspec = R"({
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
    "attributes": {"system": {"duration": 10.0}}
})";

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
    // Report the errno of the first failure.
    if (rq->writers->emit (o) < 0 && rc == 0) {
        rc = -1;
        saved_errno = errno;
    }
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

static int test_reserve_after_planned_end ()
{
    std::string params =
        "{\"load_format\": \"jgf\", \"matcher_policy\": \"high\", "
        "\"match_format\": \"rv1_nosched\", \"matcher_name\": \"CA\"}";

    resource_query_t *rq = nullptr;
    try {
        rq = new resource_query_t (tiny_jgf, params);
    } catch (...) {
        BAIL_OUT ("couldn't create resource_query_t");
    }

    void *h = static_cast<void *> (rq);
    bool reserved = false;
    std::string R;
    int64_t at = 0;
    double ov = 0.0;

    // Job 1 takes one of the two cores for ten seconds. The test does not
    // remove it, so it holds the core past its planned end.
    int rc =
        reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, one_core_jobspec, 1, reserved, R, at, ov);
    ok (rc == 0, "job 1 gets one core at t=0");

    errno = 0;
    at = 20;
    rc = run_at (rq, two_core_jobspec, MATCH_ALLOCATE, 2, at, R);
    ok (rc < 0 && errno == EBUSY, "a two-core request at t=20 is refused while job 1 holds a core");

    // Before the fix this failed with EBUSY, because the planners hold no
    // scheduled point after t=10, the planned end of job 1.
    errno = 0;
    at = 20;
    rc = run_at (rq, two_core_jobspec, MATCH_ALLOCATE_ORELSE_RESERVE, 2, at, R);
    ok (rc == 0 && at == 21, "a two-core request with reservation at t=20 is reserved at t=21");

    // The reservation holds both cores over [21, 31), so the next free
    // window for two cores starts at 31.
    errno = 0;
    at = 20;
    rc = run_at (rq, two_core_jobspec, MATCH_WITHOUT_ALLOCATING_FUTURE, 5, at, R);
    ok (rc == 0 && at == 31,
        "a two-core match without allocation at t=20 finds t=31 while job 2 is reserved");
    ok (rq->db->metadata.by_jobid.find (2) != rq->db->metadata.by_jobid.end (),
        "the graph holds state of job 2 while it is reserved");

    ok (reapi_cli_t::cancel (h, 2, false) == 0, "the reservation of job 2 is removed");

    ok (rq->db->metadata.by_jobid.find (2) == rq->db->metadata.by_jobid.end (),
        "the graph holds no state of job 2 after the cancel");

    errno = 0;
    at = 20;
    rc = run_at (rq, two_core_jobspec, MATCH_WITHOUT_ALLOCATING_FUTURE, 3, at, R);
    ok (rc == 0 && at == 21, "a two-core match without allocation at t=20 finds t=21");

    // A match without allocation writes nothing to the graph.
    ok (rq->db->metadata.by_jobid.find (3) == rq->db->metadata.by_jobid.end (),
        "the graph holds no state of job 3 after the match without allocation");
    ok (rq->db->metadata.by_jobid.find (5) == rq->db->metadata.by_jobid.end (),
        "the graph holds no state of job 5 after the match without allocation");

    ok (reapi_cli_t::cancel (h, 1, false) == 0, "job 1 is removed");

    errno = 0;
    at = 20;
    rc = run_at (rq, two_core_jobspec, MATCH_ALLOCATE, 4, at, R);
    ok (rc == 0 && at == 20, "a two-core request at t=20 is allocated now on the free node");

    delete rq;
    return 0;
}

int main (int argc, char *argv[])
{
    plan (NO_PLAN);

    test_reserve_after_planned_end ();

    done_testing ();
    return EXIT_SUCCESS;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
