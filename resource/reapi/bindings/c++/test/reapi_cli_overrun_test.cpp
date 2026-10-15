/*****************************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

/* Make sure that an immediate exclusive request is refused for a vertex
 * when another job holds a descendant of the vertex. Such a job has no
 * entry on the vertex, thus by_excl () at the vertex sees nothing, but
 * the update walk of an exclusive request gives every descendant of the
 * vertex to the new job, also the descendant that the first job holds.
 * by_shadow () refuses the vertex with the count
 * schedule.subtree_allocations, the number of entries in the subtree of
 * the vertex. resource-query cannot do this test, because it matches at
 * time 0 only. The traverser API accepts the match time, so this test
 * matches after the planned end of the first job, as if the job (or its
 * housekeeping) continues after its walltime.
 *
 * The tests cover a held core below a node, a held node below a rack, the
 * same states after a reload with each of the two readers, the same
 * states under the nodex policies, the state after a partial release, and
 * the state after a shrink. A shrink by rank also runs on a graph whose
 * core has no rank of its own, because such a core stays in the graph
 * while its node goes, and the count of the rack must still lose it. A
 * A shrink that fails, because one of its roots has no containment
 * parent, must leave every count, every entry and every out-edge as it
 * was, and a later shrink of a root with a parent must still work. A
 * reservation and a probe of a future match must pass, because they are
 * not immediate requests. A failed call of the rv1exec reader must roll
 * the counts back to the value that they had. Two jobs on one node must
 * keep the node refused until both are gone. A copy of the graph must
 * carry the counts, and a change on the copy must not reach the
 * original. Each test also shows that the refusal takes no free resource
 * away: another node, or another rack, or the released node still serves
 * the request, and the state after a full cancel is clean. A helper
 * recomputes subtree_allocations for every vertex of the containment
 * subsystem from the live maps and compares it with the stored value,
 * after each change of the state.
 */

extern "C" {
#if HAVE_CONFIG_H
#include <config.h>
#endif
}

#include <cerrno>
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

// One rack with three nodes, where the core of node2 has no broker rank.
// A shrink of rank 2 names node2 only, thus the core of node2 stays in
// the graph with the entry of its job, and the count of the rack must
// still lose that entry.
static const char *rankless_core_jgf = R"({
    "graph": {
        "nodes": [
            {"id": "0", "metadata": {"type": "cluster", "basename": "tiny", "name": "tiny0", "id": 0, "rank": -1, "size": 1, "paths": {"containment": "/tiny0"}}},
            {"id": "1", "metadata": {"type": "rack", "basename": "rack", "name": "rack0", "id": 0, "rank": -1, "size": 1, "paths": {"containment": "/tiny0/rack0"}}},
            {"id": "2", "metadata": {"type": "node", "basename": "node", "name": "node0", "id": 0, "rank": 0, "size": 1, "paths": {"containment": "/tiny0/rack0/node0"}}},
            {"id": "3", "metadata": {"type": "core", "basename": "core", "name": "core0", "id": 0, "rank": 0, "size": 1, "paths": {"containment": "/tiny0/rack0/node0/core0"}}},
            {"id": "4", "metadata": {"type": "node", "basename": "node", "name": "node1", "id": 1, "rank": 1, "size": 1, "paths": {"containment": "/tiny0/rack0/node1"}}},
            {"id": "5", "metadata": {"type": "core", "basename": "core", "name": "core0", "id": 0, "rank": 1, "size": 1, "paths": {"containment": "/tiny0/rack0/node1/core0"}}},
            {"id": "6", "metadata": {"type": "node", "basename": "node", "name": "node2", "id": 2, "rank": 2, "size": 1, "paths": {"containment": "/tiny0/rack0/node2"}}},
            {"id": "7", "metadata": {"type": "core", "basename": "core", "name": "core0", "id": 0, "rank": -1, "size": 1, "paths": {"containment": "/tiny0/rack0/node2/core0"}}}
        ],
        "edges": [
            {"source": "0", "target": "1"},
            {"source": "1", "target": "2"},
            {"source": "2", "target": "3"},
            {"source": "1", "target": "4"},
            {"source": "4", "target": "5"},
            {"source": "1", "target": "6"},
            {"source": "6", "target": "7"}
        ]
    }
})";

// A vertex of rank 5 below a node of rank 0. No reader builds this
// shape, and the test uses it only to make a root with no parent: a
// shrink of rank 0 cuts nodeA, thus nodeD stays in by_rank with no
// parent, and a later shrink of rank 5 must fail and change nothing.
// The outer id of nodeA is below the one of nodeB and the one of
// nodeC. by_outedges of the rack orders an edge of equal weight by that
// id, from high to low, thus nodeA and nodeB each leave the map before
// its last entry. The removal of the last entry reaches a defect of
// remove_metadata_outedges (), which another change corrects.
static const char *nested_rank_jgf = R"({
    "graph": {
        "nodes": [
            {"id": "0", "metadata": {"type": "cluster", "basename": "tiny", "name": "tiny0", "id": 0, "rank": -1, "size": 1, "paths": {"containment": "/tiny0"}}},
            {"id": "1", "metadata": {"type": "rack", "basename": "rack", "name": "rack0", "id": 0, "rank": -1, "size": 1, "paths": {"containment": "/tiny0/rack0"}}},
            {"id": "2", "metadata": {"type": "node", "basename": "node", "name": "nodeA", "id": 0, "rank": 0, "size": 1, "paths": {"containment": "/tiny0/rack0/nodeA"}}},
            {"id": "3", "metadata": {"type": "node", "basename": "node", "name": "nodeD", "id": 3, "rank": 5, "size": 1, "paths": {"containment": "/tiny0/rack0/nodeA/nodeD"}}},
            {"id": "4", "metadata": {"type": "core", "basename": "core", "name": "core0", "id": 0, "rank": 5, "size": 1, "paths": {"containment": "/tiny0/rack0/nodeA/nodeD/core0"}}},
            {"id": "5", "metadata": {"type": "node", "basename": "node", "name": "nodeC", "id": 2, "rank": 2, "size": 1, "paths": {"containment": "/tiny0/rack0/nodeC"}}},
            {"id": "6", "metadata": {"type": "core", "basename": "core", "name": "core0", "id": 0, "rank": 2, "size": 1, "paths": {"containment": "/tiny0/rack0/nodeC/core0"}}},
            {"id": "7", "metadata": {"type": "node", "basename": "node", "name": "nodeB", "id": 1, "rank": 1, "size": 1, "paths": {"containment": "/tiny0/rack0/nodeB"}}},
            {"id": "8", "metadata": {"type": "core", "basename": "core", "name": "core0", "id": 0, "rank": 1, "size": 1, "paths": {"containment": "/tiny0/rack0/nodeB/core0"}}}
        ],
        "edges": [
            {"source": "0", "target": "1"},
            {"source": "1", "target": "2"},
            {"source": "2", "target": "3"},
            {"source": "3", "target": "4"},
            {"source": "1", "target": "5"},
            {"source": "5", "target": "6"},
            {"source": "1", "target": "7"},
            {"source": "7", "target": "8"}
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

// One node with two cores, for the shadow tests. A job that holds one
// core of the node leaves the other core free.
static const char *node_jgf = R"({
    "graph": {
        "nodes": [
            {"id": "0", "metadata": {"type": "cluster", "basename": "tiny", "name": "tiny0", "id": 0, "rank": -1, "size": 1, "paths": {"containment": "/tiny0"}}},
            {"id": "1", "metadata": {"type": "node", "basename": "node", "name": "node0", "id": 0, "rank": 0, "size": 1, "paths": {"containment": "/tiny0/node0"}}},
            {"id": "2", "metadata": {"type": "core", "basename": "core", "name": "core0", "id": 0, "rank": 0, "size": 1, "paths": {"containment": "/tiny0/node0/core0"}}},
            {"id": "3", "metadata": {"type": "core", "basename": "core", "name": "core1", "id": 1, "rank": 0, "size": 1, "paths": {"containment": "/tiny0/node0/core1"}}}
        ],
        "edges": [
            {"source": "0", "target": "1"},
            {"source": "1", "target": "2"},
            {"source": "1", "target": "3"}
        ]
    }
})";

// Two nodes with two cores each, for the partial release tests
static const char *two_node_jgf = R"({
    "graph": {
        "nodes": [
            {"id": "0", "metadata": {"type": "cluster", "basename": "tiny", "name": "tiny0", "id": 0, "rank": -1, "size": 1, "paths": {"containment": "/tiny0"}}},
            {"id": "1", "metadata": {"type": "node", "basename": "node", "name": "node0", "id": 0, "rank": 0, "size": 1, "paths": {"containment": "/tiny0/node0"}}},
            {"id": "2", "metadata": {"type": "core", "basename": "core", "name": "core0", "id": 0, "rank": 0, "size": 1, "paths": {"containment": "/tiny0/node0/core0"}}},
            {"id": "3", "metadata": {"type": "core", "basename": "core", "name": "core1", "id": 1, "rank": 0, "size": 1, "paths": {"containment": "/tiny0/node0/core1"}}},
            {"id": "4", "metadata": {"type": "node", "basename": "node", "name": "node1", "id": 1, "rank": 1, "size": 1, "paths": {"containment": "/tiny0/node1"}}},
            {"id": "5", "metadata": {"type": "core", "basename": "core", "name": "core0", "id": 0, "rank": 1, "size": 1, "paths": {"containment": "/tiny0/node1/core0"}}},
            {"id": "6", "metadata": {"type": "core", "basename": "core", "name": "core1", "id": 1, "rank": 1, "size": 1, "paths": {"containment": "/tiny0/node1/core1"}}}
        ],
        "edges": [
            {"source": "0", "target": "1"},
            {"source": "1", "target": "2"},
            {"source": "1", "target": "3"},
            {"source": "0", "target": "4"},
            {"source": "4", "target": "5"},
            {"source": "4", "target": "6"}
        ]
    }
})";

// One core, with no node level in the request, for 10 seconds. by_excl ()
// never runs at the node for this request.
static const char *slot_core_jobspec = R"({
    "version": 1,
    "resources": [
        {
            "type": "slot",
            "count": 1,
            "label": "task",
            "with": [{"type": "core", "count": 1}]
        }
    ],
    "tasks": [{"command": ["sleep", "0"], "slot": "task", "count": {"per_slot": 1}}],
    "attributes": {"system": {"duration": 10.0}}
})";

// One core on each of two nodes, not exclusive, for 10 seconds
static const char *shared_two_node_jobspec = R"({
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
    "attributes": {"system": {"duration": 10.0}}
})";

// Two exclusive nodes, for 10 seconds
static const char *excl_two_node_jobspec = R"({
    "version": 1,
    "resources": [
        {
            "type": "node",
            "count": 2,
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

static const char *params_nosched =
    "{\"load_format\": \"jgf\", \"matcher_policy\": \"first\", "
    "\"match_format\": \"rv1_nosched\", \"matcher_name\": \"CA\"}";
static const char *params_rv1 =
    "{\"load_format\": \"jgf\", \"matcher_policy\": \"first\", "
    "\"match_format\": \"rv1\", \"matcher_name\": \"CA\"}";
// The nodex policies make every node exclusive for every request, thus
// by_shadow () examines each node of the walk.
static const char *params_nodex =
    "{\"load_format\": \"jgf\", \"matcher_policy\": \"firstnodex\", "
    "\"match_format\": \"rv1_nosched\", \"matcher_name\": \"CA\"}";
static const char *params_nodex_rv1 =
    "{\"load_format\": \"jgf\", \"matcher_policy\": \"firstnodex\", "
    "\"match_format\": \"rv1\", \"matcher_name\": \"CA\"}";
// A rack pruning filter, for the tests with two racks
static const char *params_prune =
    "{\"load_format\": \"jgf\", \"matcher_policy\": \"first\", "
    "\"match_format\": \"rv1\", \"matcher_name\": \"CA\", "
    "\"prune_filters\": \"ALL:core,ALL:node,cluster:rack\"}";
// The same filter with the policy high, which takes the highest id
// first and keeps the order of by_outedges fixed. The policy first
// stops at the first match, thus it reweights the out-edge that it
// takes and moves that edge to the end of the map of the parent.
static const char *params_prune_high =
    "{\"load_format\": \"jgf\", \"matcher_policy\": \"high\", "
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
// for the job.
static int64_t alloc_span_count (resource_query_t *rq, const char *path, int64_t jobid)
{
    vtx_t v = vertex_at (rq, path);
    auto &sched = rq->db->resource_graph[v].schedule;
    auto it = sched.allocations.find (jobid);
    if (it == sched.allocations.end ())
        return -2;
    return planner_span_resource_count (sched.plans, it->second);
}

// Return true if the vertex at a containment path holds a reservation
// entry for the job.
static bool has_reservation (resource_query_t *rq, const char *path, int64_t jobid)
{
    auto &sched = rq->db->resource_graph[vertex_at (rq, path)].schedule;
    return sched.reservations.find (jobid) != sched.reservations.end ();
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

// Count the entries of schedule.allocations of v and of each descendant
// of v in the containment subsystem. This is the value that
// schedule.subtree_allocations must hold. The dominant subsystem of the
// matcher is the containment subsystem for every policy of this test.
static int64_t count_subtree_allocations (const resource_graph_t &g, vtx_t v, subsystem_t dom)
{
    int64_t n = static_cast<int64_t> (g[v].schedule.allocations.size ());
    auto [ei, ei_end] = boost::out_edges (v, g);

    for (; ei != ei_end; ++ei)
        if (g[*ei].subsystem == dom)
            n += count_subtree_allocations (g, boost::target (*ei, g), dom);
    return n;
}

// Walk the containment subsystem from its root, and compare the stored
// schedule.subtree_allocations of each vertex with the value that the
// live maps give. A shrink leaves the removed vertices out of the walk:
// such a vertex keeps its own map, but it has no edge, thus its stored
// value no longer describes a subtree.
static void check_subtree_allocations (const resource_graph_t &g,
                                       vtx_t v,
                                       subsystem_t dom,
                                       int &mismatches,
                                       std::string &first)
{
    int64_t want = count_subtree_allocations (g, v, dom);
    if (g[v].schedule.subtree_allocations != want) {
        if (mismatches == 0) {
            char buf[256];
            snprintf (buf,
                      sizeof (buf),
                      "%s: stored %jd, counted %jd",
                      g[v].name.c_str (),
                      static_cast<intmax_t> (g[v].schedule.subtree_allocations),
                      static_cast<intmax_t> (want));
            first = buf;
        }
        mismatches++;
    }
    auto [ei, ei_end] = boost::out_edges (v, g);
    for (; ei != ei_end; ++ei)
        if (g[*ei].subsystem == dom)
            check_subtree_allocations (g, boost::target (*ei, g), dom, mismatches, first);
}

static void check_subtree_allocations (resource_query_t *rq, const char *fmt, ...)
    __attribute__ ((format (printf, 2, 3)));

static void check_subtree_allocations (resource_query_t *rq, const char *fmt, ...)
{
    resource_graph_t &g = rq->db->resource_graph;
    subsystem_t dom = rq->matcher->dom_subsystem ();
    auto root = rq->db->metadata.roots.find (dom);
    if (root == rq->db->metadata.roots.end ())
        BAIL_OUT ("the graph has no containment root");
    int mismatches = 0;
    std::string first;
    char desc[256];
    va_list ap;

    check_subtree_allocations (g, root->second, dom, mismatches, first);
    va_start (ap, fmt);
    vsnprintf (desc, sizeof (desc), fmt, ap);
    va_end (ap);
    ok (mismatches == 0, "%s", desc);
    if (mismatches != 0)
        diag ("%d vertices disagree, the first is %s", mismatches, first.c_str ());
}

// Return the stored schedule.subtree_allocations of the vertex at a
// containment path.
static int64_t subtree_allocations_at (resource_query_t *rq, const char *path)
{
    return rq->db->resource_graph[vertex_at (rq, path)].schedule.subtree_allocations;
}

// A job holds one core of a node without exclusivity. The node itself
// holds no entry, thus by_excl () at the node sees nothing. An exclusive
// request for the node must still be refused while the first job runs
// past its planned end, because the update walk would give every core of
// the node to the new job.
static void test_shared_node_overrun ()
{
    resource_query_t *rq = make_rq (node_jgf, params_nosched);
    void *h = static_cast<void *> (rq);
    std::string R;
    bool reserved = false;
    int64_t at = 0;
    double ov = 0.0;
    int rc = 0;

    ok (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, core_jobspec, 1, reserved, R, at, ov) == 0,
        "shared node: job 1 gets one core at t=0");
    // The policy first takes the highest id first, thus core1.
    ok (alloc_span_count (rq, "/tiny0/node0/core1", 1) == 1, "shared node: job 1 holds core1");
    ok (alloc_span_count (rq, "/tiny0/node0/core0", 1) == -2, "shared node: core0 stays free");
    ok (alloc_span_count (rq, "/tiny0/node0", 1) == -2,
        "shared node: the node holds no entry of job 1");
    ok (subtree_allocations_at (rq, "/tiny0/node0") == 1
            && subtree_allocations_at (rq, "/tiny0") == 1,
        "shared node: the node and the cluster count one entry below them");
    check_subtree_allocations (rq, "shared node: the counts agree after the match of job 1");

    errno = 0;
    at = 5;
    rc = run_at (rq, excl_node_jobspec, MATCH_ALLOCATE, 2, at, R);
    ok (rc < 0 && errno == EBUSY, "shared node: an exclusive node request at t=5 is refused");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_node_jobspec, MATCH_ALLOCATE, 2, at, R);
    ok (rc < 0 && errno == EBUSY,
        "shared node: an exclusive node request at t=20 is refused while job 1 holds core1");
    ok (alloc_span_count (rq, "/tiny0/node0/core1", 1) == 1
            && alloc_span_count (rq, "/tiny0/node0/core1", 2) == -2
            && alloc_span_count (rq, "/tiny0/node0/core0", 2) == -2
            && alloc_span_count (rq, "/tiny0/node0", 2) == -2,
        "shared node: the refused request changes no entry");
    ok (find_is_empty (h, "jobid-tag=2"), "shared node: no vertex keeps a tag of job 2");
    check_subtree_allocations (rq, "shared node: the counts agree after the refused request");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_node_jobspec, MATCH_WITHOUT_ALLOCATING, 2, at, R);
    ok (rc < 0 && errno == EBUSY,
        "shared node: an exclusive node request without allocation at t=20 is refused");
    ok (alloc_span_count (rq, "/tiny0/node0/core1", 1) == 1
            && alloc_span_count (rq, "/tiny0/node0/core0", 2) == -2
            && alloc_span_count (rq, "/tiny0/node0", 2) == -2,
        "shared node: the request without allocation changes no entry");
    check_subtree_allocations (rq, "shared node: the counts agree after the probe");

    // A reservation is optimistic about a job that overruns, but the
    // planner of the free core still holds the span of job 1 up to its
    // planned end. At t=20 that end is in the past, thus the planner
    // offers no later point, and the request is refused.
    errno = 0;
    at = 20;
    rc = run_at (rq, excl_node_jobspec, MATCH_ALLOCATE_ORELSE_RESERVE, 2, at, R);
    ok (rc < 0 && errno == EBUSY,
        "shared node: an exclusive node request with reservation at t=20 is refused");
    ok (reapi_cli_t::cancel (rq, 2, true) == 0, "shared node: the reservation, if any, is removed");
    check_subtree_allocations (rq, "shared node: the counts agree after the reservation attempt");

    // A request at t=5 still ends before the planned end of job 1, thus
    // the planner offers the node from t=10 on. The new check must let
    // that reservation through: it is not an immediate request.
    errno = 0;
    at = 5;
    rc = run_at (rq, excl_node_jobspec, MATCH_ALLOCATE_ORELSE_RESERVE, 2, at, R);
    ok (rc == 0 && at == 10, "shared node: an exclusive node request at t=5 is reserved for t=10");
    ok (has_reservation (rq, "/tiny0/node0", 2) && has_reservation (rq, "/tiny0/node0/core0", 2)
            && has_reservation (rq, "/tiny0/node0/core1", 2),
        "shared node: the node and both cores hold a reservation of job 2");
    ok (alloc_span_count (rq, "/tiny0/node0", 2) == -2
            && alloc_span_count (rq, "/tiny0/node0/core0", 2) == -2
            && alloc_span_count (rq, "/tiny0/node0/core1", 2) == -2,
        "shared node: the reservation leaves no allocation of job 2");
    ok (subtree_allocations_at (rq, "/tiny0/node0") == 1,
        "shared node: the reservation does not change the count of the node");
    check_subtree_allocations (rq, "shared node: the counts agree after the reservation");

    ok (reapi_cli_t::cancel (rq, 2, true) == 0, "shared node: the reservation is removed");
    ok (subtree_allocations_at (rq, "/tiny0/node0") == 1,
        "shared node: the cancel of the reservation does not change the count");
    check_subtree_allocations (rq,
                               "shared node: the counts agree after the cancel of the reservation");

    // A probe of a future match writes nothing at all.
    errno = 0;
    at = 5;
    rc = run_at (rq, excl_node_jobspec, MATCH_WITHOUT_ALLOCATING_FUTURE, 2, at, R);
    ok (rc == 0 && at == 10,
        "shared node: a future probe of an exclusive node request at t=5 finds t=10");
    ok (!has_reservation (rq, "/tiny0/node0", 2) && !has_reservation (rq, "/tiny0/node0/core0", 2)
            && !has_reservation (rq, "/tiny0/node0/core1", 2)
            && alloc_span_count (rq, "/tiny0/node0", 2) == -2,
        "shared node: the future probe leaves no entry of job 2");
    ok (subtree_allocations_at (rq, "/tiny0/node0") == 1,
        "shared node: the future probe does not change the count of the node");
    check_subtree_allocations (rq, "shared node: the counts agree after the future probe");

    errno = 0;
    at = 20;
    rc = run_at (rq, core_jobspec, MATCH_ALLOCATE, 3, at, R);
    ok (rc == 0 && alloc_span_count (rq, "/tiny0/node0/core0", 3) == 1,
        "shared node: a core request at t=20 gets the free core0");
    check_subtree_allocations (rq, "shared node: the counts agree after the match of job 3");
    ok (reapi_cli_t::cancel (rq, 3, true) == 0, "shared node: the core job is removed");
    check_subtree_allocations (rq, "shared node: the counts agree after the cancel of job 3");

    ok (reapi_cli_t::cancel (h, 1, false) == 0, "shared node: the full cancel of job 1 succeeds");
    check_subtree_allocations (rq, "shared node: the counts agree after the cancel of job 1");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_node_jobspec, MATCH_ALLOCATE, 4, at, R);
    ok (rc == 0 && alloc_span_count (rq, "/tiny0/node0/core0", 4) == 1
            && alloc_span_count (rq, "/tiny0/node0/core1", 4) == 1,
        "shared node: an exclusive node request at t=20 gets both cores after the cancel");
    ok (subtree_allocations_at (rq, "/tiny0") == 3,
        "shared node: the cluster counts the node and the two cores");
    check_subtree_allocations (rq, "shared node: the counts agree after the match of job 4");

    ok (reapi_cli_t::cancel (rq, 4, true) == 0 && subtree_allocations_at (rq, "/tiny0") == 0,
        "shared node: the cancel of job 4 empties the count of the cluster");
    check_subtree_allocations (rq, "shared node: the counts agree after the cancel of job 4");
    delete rq;
}

// The refusal must not hide a free node: a second node that holds no job
// still takes an exclusive request.
static void test_alternative_node ()
{
    resource_query_t *rq = make_rq (two_node_jgf, params_nosched);
    void *h = static_cast<void *> (rq);
    std::string R;
    bool reserved = false;
    int64_t at = 0;
    double ov = 0.0;
    int rc = 0;

    ok (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, core_jobspec, 1, reserved, R, at, ov) == 0
            && R_ranks_are (R, {1}),
        "alt node: job 1 gets one core of node1 at t=0");
    check_subtree_allocations (rq, "alt node: the counts agree after the match of job 1");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_node_jobspec, MATCH_ALLOCATE, 2, at, R);
    ok (rc == 0 && R_ranks_are (R, {0}),
        "alt node: an exclusive node request at t=20 gets the free node0");
    ok (alloc_span_count (rq, "/tiny0/node1/core1", 1) == 1
            && alloc_span_count (rq, "/tiny0/node1", 2) == -2
            && alloc_span_count (rq, "/tiny0/node1/core0", 2) == -2
            && alloc_span_count (rq, "/tiny0/node1/core1", 2) == -2,
        "alt node: node1 keeps the state of job 1 only");
    check_subtree_allocations (rq, "alt node: the counts agree after the match of job 2");
    delete rq;
}

// A job holds one node of a rack exclusively. An exclusive rack request
// must be refused while that job runs past its planned end, because the
// update walk would give every node of the rack to the new job.
static void test_excl_rack_over_held_node ()
{
    resource_query_t *rq = make_rq (rack_jgf, params_nosched);
    void *h = static_cast<void *> (rq);
    std::string R;
    bool reserved = false;
    int64_t at = 0;
    double ov = 0.0;
    int rc = 0;

    ok (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, excl_node_jobspec, 1, reserved, R, at, ov)
            == 0,
        "shadow: job 1 gets one exclusive node at t=0");
    check_subtree_allocations (rq, "shadow: the counts agree after the match of job 1");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_rack_jobspec, MATCH_ALLOCATE, 2, at, R);
    ok (rc < 0 && errno == EBUSY,
        "shadow: an exclusive rack request at t=20 is refused while job 1 holds a node");
    ok (alloc_span_count (rq, "/tiny0/rack0", 2) == -2
            && alloc_span_count (rq, "/tiny0/rack0/node0", 2) == -2
            && alloc_span_count (rq, "/tiny0/rack0/node1", 2) == -2,
        "shadow: the refused rack request leaves no entry of job 2");
    check_subtree_allocations (rq, "shadow: the counts agree after the refused request");

    ok (reapi_cli_t::cancel (h, 1, false) == 0, "shadow: the full cancel of job 1 succeeds");
    check_subtree_allocations (rq, "shadow: the counts agree after the cancel of job 1");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_rack_jobspec, MATCH_ALLOCATE, 3, at, R);
    ok (rc == 0 && R_ranks_are (R, {0, 1}),
        "shadow: an exclusive rack request at t=20 gets both nodes after the cancel");
    check_subtree_allocations (rq, "shadow: the counts agree after the match of job 3");
    delete rq;
}

// The refusal of a rack must not hide the other rack.
static void test_excl_rack_alternative ()
{
    resource_query_t *rq = make_rq (two_rack_jgf, params_prune);
    void *h = static_cast<void *> (rq);
    std::string R;
    bool reserved = false;
    int64_t at = 0;
    double ov = 0.0;
    int rc = 0;

    ok (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, core_jobspec, 1, reserved, R, at, ov) == 0
            && R_ranks_are (R, {3}),
        "alt rack: job 1 gets the core of node3 in rack1 at t=0");
    check_subtree_allocations (rq, "alt rack: the counts agree after the match of job 1");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_rack_jobspec, MATCH_ALLOCATE, 2, at, R);
    ok (rc == 0 && R_ranks_are (R, {0, 1, 2}),
        "alt rack: an exclusive rack request at t=20 gets the free rack0");
    check_subtree_allocations (rq, "alt rack: the counts agree after the match of job 2");
    delete rq;
}

// The same state after a reload. The rv1exec reader replays an R with no
// scheduling key, and the jgf reader replays an R that carries one. Both
// must leave the count of the node exact, so that the refusal holds.
static void test_reload_shared_node (bool rv1_scheduling)
{
    const char *label = rv1_scheduling ? "reload rv1" : "reload nosched";
    const char *params = rv1_scheduling ? params_rv1 : params_nosched;
    resource_query_t *rq = make_rq (node_jgf, params);
    void *h = static_cast<void *> (rq);
    std::string R;
    std::string R_out;
    bool reserved = false;
    int64_t at = 0;
    double ov = 0.0;
    int rc = 0;

    if (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, core_jobspec, 1, reserved, R, at, ov) != 0)
        BAIL_OUT ("the core match for the reload failed");
    delete rq;

    rq = make_rq (node_jgf, params);
    h = static_cast<void *> (rq);
    at = 0;
    ok (reapi_cli_t::update_allocate (h, 1, R, at, ov, R_out) == 0,
        "%s: job 1 is reloaded with one core of the node",
        label);
    ok (alloc_span_count (rq, "/tiny0/node0/core1", 1) == 1
            && alloc_span_count (rq, "/tiny0/node0/core0", 1) == -2
            && alloc_span_count (rq, "/tiny0/node0", 1) == -2,
        "%s: the reloaded job holds core1 and the node holds no entry",
        label);
    ok (subtree_allocations_at (rq, "/tiny0/node0") == 1,
        "%s: the node counts one entry below it",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the reload", label);

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_node_jobspec, MATCH_ALLOCATE, 2, at, R_out);
    ok (rc < 0 && errno == EBUSY, "%s: an exclusive node request at t=20 is refused", label);

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_node_jobspec, MATCH_WITHOUT_ALLOCATING, 2, at, R_out);
    ok (rc < 0 && errno == EBUSY,
        "%s: an exclusive node request without allocation at t=20 is refused",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the refused requests", label);

    errno = 0;
    at = 20;
    rc = run_at (rq, core_jobspec, MATCH_ALLOCATE, 3, at, R_out);
    ok (rc == 0 && alloc_span_count (rq, "/tiny0/node0/core0", 3) == 1,
        "%s: a core request at t=20 gets the free core0",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the match of job 3", label);
    ok (reapi_cli_t::cancel (rq, 3, true) == 0, "%s: the core job is removed", label);

    ok (reapi_cli_t::cancel (h, 1, false) == 0, "%s: the full cancel of job 1 succeeds", label);
    check_subtree_allocations (rq, "%s: the counts agree after the cancel of job 1", label);

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_node_jobspec, MATCH_ALLOCATE, 4, at, R_out);
    ok (rc == 0 && alloc_span_count (rq, "/tiny0/node0/core0", 4) == 1
            && alloc_span_count (rq, "/tiny0/node0/core1", 4) == 1,
        "%s: an exclusive node request at t=20 gets both cores after the cancel",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the match of job 4", label);
    delete rq;
}

// The nodex policies make every node exclusive, also for a request that
// names no node. A job that took one core under another policy and that
// a nodex instance reloads must still block the whole node.
static void test_reload_nodex_slot_core (bool rv1_scheduling)
{
    const char *label = rv1_scheduling ? "nodex reload rv1" : "nodex reload nosched";
    const char *params = rv1_scheduling ? params_rv1 : params_nosched;
    const char *params_x = rv1_scheduling ? params_nodex_rv1 : params_nodex;
    resource_query_t *rq = make_rq (node_jgf, params);
    void *h = static_cast<void *> (rq);
    std::string R;
    std::string R_out;
    bool reserved = false;
    int64_t at = 0;
    double ov = 0.0;
    int rc = 0;

    // A nodex instance cannot create a job that holds one core only, thus
    // the job comes from an instance with the policy first.
    if (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, core_jobspec, 1, reserved, R, at, ov) != 0)
        BAIL_OUT ("the core match for the nodex reload failed");
    delete rq;

    rq = make_rq (node_jgf, params_x);
    h = static_cast<void *> (rq);
    at = 0;
    ok (reapi_cli_t::update_allocate (h, 1, R, at, ov, R_out) == 0,
        "%s: job 1 is reloaded with one core of the node",
        label);
    ok (alloc_span_count (rq, "/tiny0/node0/core1", 1) == 1
            && alloc_span_count (rq, "/tiny0/node0/core0", 1) == -2
            && alloc_span_count (rq, "/tiny0/node0", 1) == -2,
        "%s: the reloaded job holds core1 and the node holds no entry",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the reload", label);

    // The request names no node, thus by_excl () never runs at the node.
    errno = 0;
    at = 20;
    rc = run_at (rq, slot_core_jobspec, MATCH_ALLOCATE, 2, at, R_out);
    ok (rc < 0 && errno == EBUSY, "%s: a core request at t=20 is refused", label);
    ok (alloc_span_count (rq, "/tiny0/node0/core1", 1) == 1
            && alloc_span_count (rq, "/tiny0/node0/core0", 2) == -2
            && alloc_span_count (rq, "/tiny0/node0", 2) == -2,
        "%s: the refused core request changes no entry",
        label);

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_node_jobspec, MATCH_ALLOCATE, 2, at, R_out);
    ok (rc < 0 && errno == EBUSY, "%s: an exclusive node request at t=20 is refused", label);
    check_subtree_allocations (rq, "%s: the counts agree after the refused requests", label);

    ok (reapi_cli_t::cancel (h, 1, false) == 0, "%s: the full cancel of job 1 succeeds", label);
    check_subtree_allocations (rq, "%s: the counts agree after the cancel of job 1", label);

    errno = 0;
    at = 20;
    rc = run_at (rq, slot_core_jobspec, MATCH_ALLOCATE, 3, at, R_out);
    ok (rc == 0 && alloc_span_count (rq, "/tiny0/node0/core0", 3) == 1
            && alloc_span_count (rq, "/tiny0/node0/core1", 3) == 1
            && alloc_span_count (rq, "/tiny0/node0", 3) == 1,
        "%s: a core request at t=20 takes the whole node after the cancel",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the match of job 3", label);
    delete rq;
}

// A job holds one core of each of two nodes. A partial release frees one
// node. The other node keeps the held core, thus an exclusive request for
// the two nodes must be refused while the job runs past its planned end,
// and the released node must be available.
static void test_partial_release_shared_then_overrun ()
{
    resource_query_t *rq = make_rq (two_node_jgf, params_nosched);
    void *h = static_cast<void *> (rq);
    std::string R;
    bool reserved = false;
    bool full_removal = true;
    int64_t at = 0;
    double ov = 0.0;
    int rc = 0;

    ok (reapi_cli_t::match_allocate (h,
                                     MATCH_ALLOCATE,
                                     shared_two_node_jobspec,
                                     1,
                                     reserved,
                                     R,
                                     at,
                                     ov)
                == 0
            && R_ranks_are (R, {0, 1}),
        "partial shared: job 1 gets one core of each node at t=0");
    ok (subtree_allocations_at (rq, "/tiny0") == 2,
        "partial shared: the cluster counts the two cores");
    check_subtree_allocations (rq, "partial shared: the counts agree after the match of job 1");

    full_removal = true;
    ok (reapi_cli_t::cancel (h, 1, rank1_R, false, full_removal) == 0 && !full_removal,
        "partial shared: the release of rank 1 is not a full removal");
    ok (alloc_span_count (rq, "/tiny0/node1/core0", 1) == -2
            && alloc_span_count (rq, "/tiny0/node1/core1", 1) == -2,
        "partial shared: node1 holds no entry of job 1");
    ok (subtree_allocations_at (rq, "/tiny0") == 1,
        "partial shared: the cluster counts the core of node0 only");
    check_subtree_allocations (rq, "partial shared: the counts agree after the release of rank 1");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_two_node_jobspec, MATCH_ALLOCATE, 2, at, R);
    ok (rc < 0 && errno == EBUSY,
        "partial shared: an exclusive request for both nodes at t=20 is refused");
    ok (find_is_empty (h, "jobid-tag=2"), "partial shared: no vertex keeps a tag of job 2");
    check_subtree_allocations (rq, "partial shared: the counts agree after the refused request");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_node_jobspec, MATCH_ALLOCATE, 2, at, R);
    ok (rc == 0 && R_ranks_are (R, {1}),
        "partial shared: an exclusive node request at t=20 gets the released node1");
    check_subtree_allocations (rq, "partial shared: the counts agree after the match of job 2");
    ok (reapi_cli_t::cancel (rq, 2, true) == 0, "partial shared: job 2 is removed");

    full_removal = false;
    ok (reapi_cli_t::cancel (h, 1, rank0_R, false, full_removal) == 0 && full_removal,
        "partial shared: the release of rank 0 completes the cancel of job 1");
    check_subtree_allocations (rq, "partial shared: the counts agree after the release of rank 0");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_two_node_jobspec, MATCH_ALLOCATE, 3, at, R);
    ok (rc == 0 && R_ranks_are (R, {0, 1}),
        "partial shared: an exclusive request for both nodes at t=20 succeeds after the release");
    check_subtree_allocations (rq, "partial shared: the counts agree after the match of job 3");
    delete rq;
}

// The same shape with an exclusive job: the new check must not change the
// state that by_excl () already protects.
static void test_partial_release_excl_then_overrun ()
{
    resource_query_t *rq = make_rq (two_node_jgf, params_nosched);
    void *h = static_cast<void *> (rq);
    std::string R;
    bool reserved = false;
    bool full_removal = true;
    int64_t at = 0;
    double ov = 0.0;
    int rc = 0;

    ok (reapi_cli_t::match_allocate (h,
                                     MATCH_ALLOCATE,
                                     excl_two_node_jobspec,
                                     1,
                                     reserved,
                                     R,
                                     at,
                                     ov)
                == 0
            && R_ranks_are (R, {0, 1}),
        "partial excl: job 1 gets both nodes exclusively at t=0");
    check_subtree_allocations (rq, "partial excl: the counts agree after the match of job 1");

    full_removal = true;
    ok (reapi_cli_t::cancel (h, 1, rank1_R, false, full_removal) == 0 && !full_removal,
        "partial excl: the release of rank 1 is not a full removal");
    check_subtree_allocations (rq, "partial excl: the counts agree after the release of rank 1");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_two_node_jobspec, MATCH_ALLOCATE, 2, at, R);
    ok (rc < 0 && errno == EBUSY,
        "partial excl: an exclusive request for both nodes at t=20 is refused");
    check_subtree_allocations (rq, "partial excl: the counts agree after the refused request");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_node_jobspec, MATCH_ALLOCATE, 2, at, R);
    ok (rc == 0 && R_ranks_are (R, {1}),
        "partial excl: an exclusive node request at t=20 gets the released node1");
    ok (reapi_cli_t::cancel (rq, 2, true) == 0, "partial excl: job 2 is removed");

    errno = 0;
    at = 20;
    rc = run_at (rq, core_jobspec, MATCH_ALLOCATE, 3, at, R);
    ok (rc == 0 && R_ranks_are (R, {1}),
        "partial excl: a core request at t=20 gets the released node1");
    ok (reapi_cli_t::cancel (rq, 3, true) == 0, "partial excl: job 3 is removed");
    check_subtree_allocations (rq,
                               "partial excl: the counts agree after the released node is used");

    full_removal = false;
    ok (reapi_cli_t::cancel (h, 1, rank0_R, false, full_removal) == 0 && full_removal,
        "partial excl: the release of rank 0 completes the cancel of job 1");
    check_subtree_allocations (rq, "partial excl: the counts agree after the release of rank 0");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_two_node_jobspec, MATCH_ALLOCATE, 4, at, R);
    ok (rc == 0 && R_ranks_are (R, {0, 1}),
        "partial excl: an exclusive request for both nodes at t=20 succeeds after the release");
    check_subtree_allocations (rq, "partial excl: the counts agree after the match of job 4");
    delete rq;
}

// The same state as the partial release of a shared job, but after a
// reload: the R of job 1 names rank 1 in free_ranks. Only the jgf reader
// can express a freed rank, thus this case uses the rv1 match format.
static void test_reload_partial_then_overrun ()
{
    resource_query_t *rq = make_rq (two_node_jgf, params_rv1);
    void *h = static_cast<void *> (rq);
    std::string R;
    std::string R_out;
    bool reserved = false;
    bool full_removal = true;
    int64_t at = 0;
    double ov = 0.0;
    int rc = 0;

    if (reapi_cli_t::match_allocate (h,
                                     MATCH_ALLOCATE,
                                     shared_two_node_jobspec,
                                     1,
                                     reserved,
                                     R,
                                     at,
                                     ov)
        != 0)
        BAIL_OUT ("the two-node match for the partial reload failed");
    delete rq;

    json_error_t err;
    json_t *o = json_loads (R.c_str (), 0, &err);
    json_t *scheduling = o ? json_object_get (o, "scheduling") : nullptr;
    if (!scheduling)
        BAIL_OUT ("the two-node R has no scheduling key");
    json_object_set_new (scheduling, "free_ranks", json_string ("1"));
    char *s = json_dumps (o, JSON_COMPACT);
    std::string R_free1 = s;
    free (s);
    json_decref (o);

    rq = make_rq (two_node_jgf, params_rv1);
    h = static_cast<void *> (rq);
    at = 0;
    ok (reapi_cli_t::update_allocate (h, 1, R_free1, at, ov, R_out) == 0
            && R_ranks_are (R_out, {0}),
        "reload partial: job 1 is reloaded with rank 1 in free_ranks");
    ok (alloc_span_count (rq, "/tiny0/node0/core1", 1) == 1
            && alloc_span_count (rq, "/tiny0/node1/core0", 1) == -2
            && alloc_span_count (rq, "/tiny0/node1/core1", 1) == -2,
        "reload partial: the reloaded job holds one core of node0 only");
    ok (subtree_allocations_at (rq, "/tiny0") == 1,
        "reload partial: the cluster counts the core of node0 only");
    check_subtree_allocations (rq, "reload partial: the counts agree after the reload");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_two_node_jobspec, MATCH_ALLOCATE, 2, at, R_out);
    ok (rc < 0 && errno == EBUSY,
        "reload partial: an exclusive request for both nodes at t=20 is refused");
    check_subtree_allocations (rq, "reload partial: the counts agree after the refused request");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_node_jobspec, MATCH_ALLOCATE, 2, at, R_out);
    ok (rc == 0 && R_ranks_are (R_out, {1}),
        "reload partial: an exclusive node request at t=20 gets the released node1");
    ok (reapi_cli_t::cancel (rq, 2, true) == 0, "reload partial: job 2 is removed");
    check_subtree_allocations (rq, "reload partial: the counts agree after the match of job 2");

    full_removal = false;
    ok (reapi_cli_t::cancel (h, 1, rank0_R, false, full_removal) == 0 && full_removal,
        "reload partial: the release of rank 0 completes the cancel of job 1");
    check_subtree_allocations (rq, "reload partial: the counts agree after the release of rank 0");

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_two_node_jobspec, MATCH_ALLOCATE, 3, at, R_out);
    ok (rc == 0 && R_ranks_are (R_out, {0, 1}),
        "reload partial: an exclusive request for both nodes at t=20 succeeds after the release");
    check_subtree_allocations (rq, "reload partial: the counts agree after the match of job 3");
    delete rq;
}

// A shrink removes the subtree that holds a job. The count of the
// ancestors must lose the entries of the removed vertices, so that an
// exclusive request for a remaining ancestor is not refused forever.
static void test_shrink_keeps_count ()
{
    const char *label = "shrink";
    const char *target = "/tiny0/rack1";
    resource_query_t *rq = make_rq (two_rack_jgf, params_prune);
    void *h = static_cast<void *> (rq);
    std::string R;
    bool reserved = false;
    int64_t at = 0;
    double ov = 0.0;
    int rc = 0;

    ok (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, core_jobspec, 1, reserved, R, at, ov) == 0
            && R_ranks_are (R, {3}),
        "%s: job 1 gets the core of node3 in rack1 at t=0",
        label);
    ok (subtree_allocations_at (rq, "/tiny0") == 1,
        "%s: the cluster counts the core of job 1",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the match of job 1", label);

    ok (reapi_cli_t::remove_subgraph (h, target) == 0, "%s: the subgraph is removed", label);
    ok (subtree_allocations_at (rq, "/tiny0/rack0") == 0
            && subtree_allocations_at (rq, "/tiny0") == 0,
        "%s: the cluster and rack0 count no entry",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the shrink", label);

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_rack_jobspec, MATCH_ALLOCATE, 2, at, R);
    ok (rc == 0 && R_ranks_are (R, {0, 1, 2}),
        "%s: an exclusive rack request at t=20 gets the three nodes of rack0",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the match of job 2", label);
    delete rq;
}

// A shrink by rank removes the vertices of the rank only. A descendant
// with no rank of its own stays in the graph with the entry of its job.
// The subtraction must still use the count of the removed root, so that
// the rack does not keep a number that no live entry explains.
static void test_rank_shrink_rankless_descendant ()
{
    const char *label = "rank shrink";
    resource_query_t *rq = make_rq (rankless_core_jgf, params_prune_high);
    void *h = static_cast<void *> (rq);
    std::string R;
    bool reserved = false;
    int64_t at = 0;
    double ov = 0.0;
    int rc = 0;

    // The policy high takes the highest id first, thus node2, the node
    // with the core that has no rank. The test removes the first
    // out-edge of the rack, because the removal of the last out-edge
    // reaches a defect of remove_metadata_outedges (), which another
    // change corrects.
    ok (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, core_jobspec, 1, reserved, R, at, ov) == 0
            && alloc_span_count (rq, "/tiny0/rack0/node2/core0", 1) == 1,
        "%s: job 1 gets the core of node2, which has no rank",
        label);
    ok (subtree_allocations_at (rq, "/tiny0/rack0") == 1,
        "%s: rack0 counts the core of job 1",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the match of job 1", label);

    // The module shrinks a rank with these three calls, in this order.
    ok (rq->traverser->remove (std::set<int64_t>{2}) == 0
            && rq->traverser->remove_subgraph (std::set<int64_t>{2}) == 0
            && rq->traverser->initialize () == 0,
        "%s: the shrink of rank 2 succeeds",
        label);
    ok (subtree_allocations_at (rq, "/tiny0/rack0") == 0
            && subtree_allocations_at (rq, "/tiny0") == 0,
        "%s: rack0 and the cluster count no entry after the shrink",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the shrink", label);

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_rack_jobspec, MATCH_ALLOCATE, 2, at, R);
    ok (rc == 0 && R_ranks_are (R, {0, 1}),
        "%s: an exclusive rack request at t=20 gets the two remaining nodes",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the match of job 2", label);

    ok (reapi_cli_t::cancel (rq, 2, true) == 0 && subtree_allocations_at (rq, "/tiny0/rack0") == 0,
        "%s: the cancel of job 2 empties the count of rack0",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the cancel of job 2", label);

    // The shrink cuts the edges of the core of node2, but it leaves the
    // entry of job 1 on the core, because the core has no rank of its
    // own. The cancel of job 1 finds the core through the by_jobid index
    // and removes the entry. The count of rack0 stays zero, because the
    // core is no longer below the rack.
    ok (reapi_cli_t::cancel (h, 1, false) == 0,
        "%s: the full cancel of job 1 succeeds after the shrink",
        label);
    ok (subtree_allocations_at (rq, "/tiny0/rack0") == 0
            && subtree_allocations_at (rq, "/tiny0") == 0,
        "%s: the cancel of job 1 leaves rack0 and the cluster at zero",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the cancel of job 1", label);
    delete rq;
}

// A shrink of two ranks while a third job holds a node that stays. The
// count of the rack must keep that one entry, and lose the other two.
static void test_two_rank_shrink_other_holder ()
{
    const char *label = "two-rank shrink";
    resource_query_t *rq = make_rq (rankless_core_jgf, params_prune);
    void *h = static_cast<void *> (rq);
    std::string R;
    bool reserved = false;
    int64_t at = 0;
    double ov = 0.0;
    int rc = 0;

    // The policy first takes the highest id first, thus node2, then
    // node1, then node0. The test removes the first two out-edges of
    // the rack, because the removal of the last out-edge reaches a
    // defect of remove_metadata_outedges (), which another change
    // corrects.
    at = 0;
    ok (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, core_jobspec, 1, reserved, R, at, ov) == 0
            && alloc_span_count (rq, "/tiny0/rack0/node2/core0", 1) == 1,
        "%s: job 1 gets the core of node2, which has no rank",
        label);
    at = 0;
    ok (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, core_jobspec, 2, reserved, R, at, ov) == 0
            && alloc_span_count (rq, "/tiny0/rack0/node1/core0", 2) == 1,
        "%s: job 2 gets the core of node1",
        label);
    at = 0;
    ok (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, core_jobspec, 3, reserved, R, at, ov) == 0
            && alloc_span_count (rq, "/tiny0/rack0/node0/core0", 3) == 1,
        "%s: job 3 gets the core of node0",
        label);
    ok (subtree_allocations_at (rq, "/tiny0/rack0") == 3,
        "%s: rack0 counts the three cores",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the three matches", label);

    ok (rq->traverser->remove (std::set<int64_t>{1, 2}) == 0
            && rq->traverser->remove_subgraph (std::set<int64_t>{1, 2}) == 0
            && rq->traverser->initialize () == 0,
        "%s: the shrink of rank 1 and rank 2 succeeds",
        label);
    ok (subtree_allocations_at (rq, "/tiny0/rack0") == 1
            && subtree_allocations_at (rq, "/tiny0") == 1,
        "%s: rack0 and the cluster count the core of job 3 only",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the shrink", label);

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_rack_jobspec, MATCH_ALLOCATE, 4, at, R);
    ok (rc < 0 && errno == EBUSY,
        "%s: an exclusive rack request at t=20 is refused while job 3 holds node0",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the refused request", label);

    ok (reapi_cli_t::cancel (h, 3, false) == 0, "%s: the full cancel of job 3 succeeds", label);
    ok (subtree_allocations_at (rq, "/tiny0/rack0") == 0
            && subtree_allocations_at (rq, "/tiny0") == 0,
        "%s: rack0 and the cluster count no entry after the cancel",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the cancel of job 3", label);

    // The shrink cut off job 1 and job 2. The core of node2 keeps the
    // entry of job 1, because that core has no rank of its own. Both
    // cancels find their vertices through the by_jobid index, and
    // neither changes a count below the rack.
    ok (reapi_cli_t::cancel (h, 1, false) == 0 && reapi_cli_t::cancel (h, 2, false) == 0,
        "%s: the full cancel of job 1 and job 2 succeeds after the shrink",
        label);
    ok (subtree_allocations_at (rq, "/tiny0/rack0") == 0
            && subtree_allocations_at (rq, "/tiny0") == 0,
        "%s: the two cancels leave rack0 and the cluster at zero",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the two cancels", label);

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_rack_jobspec, MATCH_ALLOCATE, 4, at, R);
    ok (rc == 0 && R_ranks_are (R, {0}),
        "%s: an exclusive rack request at t=20 gets node0 after the cancels",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the match of job 4", label);
    delete rq;
}

// A shrink by rank fails when one of its roots has no containment
// parent. The failure must change nothing: no count, no entry, and no
// out-edge of an ancestor. The other ranks of the same call must also
// stay, thus a later shrink of such a rank must still work.
static void test_shrink_failure_changes_nothing ()
{
    const char *label = "failed shrink";
    resource_query_t *rq = make_rq (nested_rank_jgf, params_prune);
    void *h = static_cast<void *> (rq);
    std::string R;
    bool reserved = false;
    int64_t at = 0;
    double ov = 0.0;
    int64_t r = 0;
    int64_t t = 0;
    int rc = 0;

    // The policy first takes the highest id first. nodeB and nodeC hold
    // the two cores that the walk reaches, thus the two matches land
    // there. nodeD is below nodeA, and the walk of a core request never
    // reaches it, because nodeA already answers as a node.
    at = 0;
    ok (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, core_jobspec, 1, reserved, R, at, ov) == 0
            && alloc_span_count (rq, "/tiny0/rack0/nodeB/core0", 1) == 1,
        "%s: job 1 gets the core of nodeB",
        label);
    at = 0;
    ok (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, core_jobspec, 2, reserved, R, at, ov) == 0
            && alloc_span_count (rq, "/tiny0/rack0/nodeC/core0", 2) == 1,
        "%s: job 2 gets the core of nodeC",
        label);
    ok (alloc_span_count (rq, "/tiny0/rack0/nodeA/nodeD/core0", 1) == -2
            && alloc_span_count (rq, "/tiny0/rack0/nodeA/nodeD/core0", 2) == -2,
        "%s: the core of nodeD holds no entry",
        label);
    ok (subtree_allocations_at (rq, "/tiny0/rack0") == 2
            && subtree_allocations_at (rq, "/tiny0") == 2,
        "%s: rack0 and the cluster count the two cores",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the two matches", label);

    // The shrink of rank 0 cuts nodeA. nodeD keeps its entry in by_rank
    // for rank 5, because remove_graph_metadata () removes a cut vertex
    // from by_rank for its own rank only. nodeD thus has no parent.
    ok (rq->traverser->remove (std::set<int64_t>{0}) == 0
            && rq->traverser->remove_subgraph (std::set<int64_t>{0}) == 0
            && rq->traverser->initialize () == 0,
        "%s: the shrink of rank 0 succeeds",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the shrink of rank 0", label);
    r = subtree_allocations_at (rq, "/tiny0/rack0");
    t = subtree_allocations_at (rq, "/tiny0");
    ok (r == 2 && t == 2,
        "%s: rack0 and the cluster still count the cores of nodeB and nodeC",
        label);

    // rank 5 has no parent, thus the first pass of remove_subgraph ()
    // must fail before it changes anything, also for rank 1, which does
    // have a parent.
    ok (rq->traverser->remove_subgraph (std::set<int64_t>{1, 5}) == -1,
        "%s: the shrink of rank 1 and rank 5 fails, because nodeD has no parent",
        label);
    ok (subtree_allocations_at (rq, "/tiny0/rack0") == r
            && subtree_allocations_at (rq, "/tiny0") == t,
        "%s: the failed shrink changes no count",
        label);
    ok (alloc_span_count (rq, "/tiny0/rack0/nodeB/core0", 1) == 1
            && alloc_span_count (rq, "/tiny0/rack0/nodeC/core0", 2) == 1,
        "%s: the failed shrink leaves the entry of each core",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the failed shrink", label);

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_rack_jobspec, MATCH_ALLOCATE, 3, at, R);
    ok (rc < 0 && errno == EBUSY,
        "%s: an exclusive rack request at t=20 is refused after the failed shrink",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the refused request", label);

    // The same shrink without rank 5 must work, because nodeB has a
    // parent. It takes the core of nodeB out of the count of rack0.
    ok (rq->traverser->remove (std::set<int64_t>{1}) == 0
            && rq->traverser->remove_subgraph (std::set<int64_t>{1}) == 0
            && rq->traverser->initialize () == 0,
        "%s: the shrink of rank 1 alone succeeds",
        label);
    ok (subtree_allocations_at (rq, "/tiny0/rack0") == t - 1,
        "%s: rack0 loses the core of nodeB",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the shrink of rank 1", label);

    ok (reapi_cli_t::cancel (h, 1, false) == 0 && reapi_cli_t::cancel (h, 2, false) == 0,
        "%s: the full cancel of job 1 and job 2 succeeds",
        label);
    ok (subtree_allocations_at (rq, "/tiny0/rack0") == 0,
        "%s: rack0 counts no entry after the cancels",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the cancels", label);

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_rack_jobspec, MATCH_ALLOCATE, 4, at, R);
    ok (rc == 0 && R_ranks_are (R, {2}),
        "%s: an exclusive rack request at t=20 gets nodeC after the cancels",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the match of job 4", label);
    delete rq;
}

// The rv1exec reader installs the vertices of an R one by one. A core
// that another job holds stops it, and undo_vertices () takes back the
// entries that it already wrote. The counts must return to the value
// that they had before the failed call.
static void test_reader_rollback_keeps_count ()
{
    const char *label = "rollback";
    // An R for job 2 that names both cores of node1, one of which job 1
    // holds. The reader reaches the held core and fails.
    const char *R_bad =
        R"({"version":1,"execution":{"R_lite":[{"rank":"1","children":{"core":"0-1"}}],"starttime":0,"expiration":10,"nodelist":["node1"]}})";
    // The same shape on node0, which no job holds.
    const char *R_good =
        R"({"version":1,"execution":{"R_lite":[{"rank":"0","children":{"core":"0-1"}}],"starttime":0,"expiration":10,"nodelist":["node0"]}})";
    resource_query_t *rq = make_rq (two_node_jgf, params_nosched);
    void *h = static_cast<void *> (rq);
    std::string R;
    std::string R_out;
    bool reserved = false;
    int64_t at = 0;
    double ov = 0.0;
    int rc = 0;

    // The policy first takes the highest id first, thus core1 of node1.
    ok (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, core_jobspec, 1, reserved, R, at, ov) == 0
            && alloc_span_count (rq, "/tiny0/node1/core1", 1) == 1,
        "%s: job 1 holds core1 of node1",
        label);
    ok (subtree_allocations_at (rq, "/tiny0/node1") == 1
            && subtree_allocations_at (rq, "/tiny0") == 1,
        "%s: node1 and the cluster count the core of job 1",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the match of job 1", label);

    at = 0;
    rc = reapi_cli_t::update_allocate (h, 2, R_bad, at, ov, R_out);
    ok (rc != 0, "%s: an R for job 2 over the held core of node1 fails", label);
    ok (find_is_empty (h, "jobid-alloc=2"), "%s: no vertex holds an entry of job 2", label);
    ok (subtree_allocations_at (rq, "/tiny0/node1") == 1
            && subtree_allocations_at (rq, "/tiny0") == 1
            && subtree_allocations_at (rq, "/tiny0/node0") == 0,
        "%s: the failed call leaves every count as it was",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the failed call", label);

    at = 0;
    ok (reapi_cli_t::update_allocate (h, 2, R_good, at, ov, R_out) == 0,
        "%s: an R for job 2 on the free node0 succeeds",
        label);
    ok (subtree_allocations_at (rq, "/tiny0/node0") == 3,
        "%s: node0 counts itself and its two cores",
        label);
    ok (subtree_allocations_at (rq, "/tiny0") == 4,
        "%s: the cluster counts the core of job 1 and the three entries of job 2",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the match of job 2", label);

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_node_jobspec, MATCH_ALLOCATE, 3, at, R_out);
    ok (rc < 0 && errno == EBUSY,
        "%s: an exclusive node request at t=20 is refused while both nodes are held",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the refused request", label);

    ok (reapi_cli_t::cancel (h, 1, false) == 0 && reapi_cli_t::cancel (h, 2, false) == 0,
        "%s: the full cancel of job 1 and job 2 succeeds",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the cancels", label);

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_node_jobspec, MATCH_ALLOCATE, 3, at, R_out);
    ok (rc == 0 && R_ranks_are (R_out, {1}),
        "%s: an exclusive node request at t=20 succeeds after the cancels",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the match of job 3", label);
    delete rq;
}

// Two jobs hold one core each of the same node. The node must stay
// refused until both jobs are gone, thus each cancel must subtract one
// entry only.
static void test_two_tenants ()
{
    const char *label = "two tenants";
    resource_query_t *rq = make_rq (node_jgf, params_nosched);
    void *h = static_cast<void *> (rq);
    std::string R;
    bool reserved = false;
    int64_t at = 0;
    double ov = 0.0;
    int rc = 0;

    at = 0;
    ok (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, core_jobspec, 1, reserved, R, at, ov) == 0
            && alloc_span_count (rq, "/tiny0/node0/core1", 1) == 1,
        "%s: job 1 gets core1",
        label);
    at = 0;
    ok (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, core_jobspec, 2, reserved, R, at, ov) == 0
            && alloc_span_count (rq, "/tiny0/node0/core0", 2) == 1,
        "%s: job 2 gets core0",
        label);
    ok (subtree_allocations_at (rq, "/tiny0/node0") == 2,
        "%s: the node counts the two cores",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the two matches", label);

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_node_jobspec, MATCH_ALLOCATE, 3, at, R);
    ok (rc < 0 && errno == EBUSY,
        "%s: an exclusive node request at t=20 is refused while both jobs run",
        label);

    ok (reapi_cli_t::cancel (h, 1, false) == 0 && subtree_allocations_at (rq, "/tiny0/node0") == 1,
        "%s: the cancel of job 1 leaves one entry below the node",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the cancel of job 1", label);

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_node_jobspec, MATCH_ALLOCATE, 3, at, R);
    ok (rc < 0 && errno == EBUSY,
        "%s: an exclusive node request at t=20 is still refused while job 2 runs",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the second refused request", label);

    ok (reapi_cli_t::cancel (h, 2, false) == 0 && subtree_allocations_at (rq, "/tiny0/node0") == 0,
        "%s: the cancel of job 2 empties the count of the node",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the cancel of job 2", label);

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_node_jobspec, MATCH_ALLOCATE, 3, at, R);
    ok (rc == 0 && alloc_span_count (rq, "/tiny0/node0/core0", 3) == 1
            && alloc_span_count (rq, "/tiny0/node0/core1", 3) == 1,
        "%s: an exclusive node request at t=20 gets both cores after the cancels",
        label);
    check_subtree_allocations (rq, "%s: the counts agree after the match of job 3", label);
    delete rq;
}

// The copy constructor of resource_query_t deep-copies the graph. The
// copy must carry the counts, and a change on one graph must not reach
// the other.
static void test_clone_keeps_count ()
{
    const char *label = "clone";
    resource_query_t *rq = make_rq (node_jgf, params_nosched);
    void *h = static_cast<void *> (rq);
    std::string R;
    bool reserved = false;
    int64_t at = 0;
    double ov = 0.0;
    int rc = 0;

    ok (reapi_cli_t::match_allocate (h, MATCH_ALLOCATE, core_jobspec, 1, reserved, R, at, ov) == 0
            && alloc_span_count (rq, "/tiny0/node0/core1", 1) == 1,
        "%s: job 1 holds core1",
        label);

    resource_query_t copy (*rq);
    ok (subtree_allocations_at (&copy, "/tiny0/node0") == 1,
        "%s: the copy counts the core of job 1",
        label);
    check_subtree_allocations (&copy, "%s: the counts of the copy agree", label);

    ok (reapi_cli_t::cancel (&copy, 1, false) == 0, "%s: the cancel on the copy succeeds", label);
    ok (subtree_allocations_at (&copy, "/tiny0/node0") == 0,
        "%s: the copy counts no entry after the cancel",
        label);
    ok (subtree_allocations_at (rq, "/tiny0/node0") == 1
            && alloc_span_count (rq, "/tiny0/node0/core1", 1) == 1,
        "%s: the cancel on the copy leaves the original as it was",
        label);
    check_subtree_allocations (&copy, "%s: the counts of the copy agree after the cancel", label);
    check_subtree_allocations (rq, "%s: the counts of the original agree after the cancel", label);

    errno = 0;
    at = 20;
    rc = run_at (&copy, excl_node_jobspec, MATCH_ALLOCATE, 2, at, R);
    ok (rc == 0 && R_ranks_are (R, {0}),
        "%s: an exclusive node request at t=20 succeeds on the copy",
        label);
    check_subtree_allocations (&copy, "%s: the counts of the copy agree after the match", label);

    errno = 0;
    at = 20;
    rc = run_at (rq, excl_node_jobspec, MATCH_ALLOCATE, 2, at, R);
    ok (rc < 0 && errno == EBUSY,
        "%s: an exclusive node request at t=20 is refused on the original",
        label);
    check_subtree_allocations (rq, "%s: the counts of the original agree after the request", label);
    delete rq;
}

int main (int argc, char *argv[])
{
    plan (NO_PLAN);
    test_shared_node_overrun ();
    test_alternative_node ();
    test_excl_rack_over_held_node ();
    test_excl_rack_alternative ();
    test_reload_shared_node (false);
    test_reload_shared_node (true);
    test_reload_nodex_slot_core (false);
    test_reload_nodex_slot_core (true);
    test_partial_release_shared_then_overrun ();
    test_partial_release_excl_then_overrun ();
    test_reload_partial_then_overrun ();
    test_shrink_keeps_count ();
    test_rank_shrink_rankless_descendant ();
    test_two_rank_shrink_other_holder ();
    test_shrink_failure_changes_nothing ();
    test_reader_rollback_keeps_count ();
    test_two_tenants ();
    test_clone_keeps_count ();

    done_testing ();
    return 0;
}
