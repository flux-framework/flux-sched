/*****************************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

/* Make sure that the functions that change schedule.allocations keep the
 * invariant of subtree_allocations: the value on a vertex is the sum of
 * allocations.size () over the vertex and its descendants in the
 * containment subsystem. Edges of other subsystems must not count. The
 * walk to the ancestors takes the first containment in-edge of a vertex,
 * thus the test adds the in-edge of the other subsystem in each of the
 * two orders.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <cstdint>
#include "resource_graph.hpp"
#include "src/common/libtap/tap.h"

using namespace Flux::resource_model;

// cluster -> node -> {core0, core1}, and a power edge pdu -> node, which
// is not in the containment subsystem. With power_first the power edge
// comes before the containment edges, thus it is the first in-edge of
// the node.
struct graph_fixture_t {
    resource_graph_t g;
    vtx_t cluster, node, core0, core1, pdu;

    graph_fixture_t (bool power_first = false)
    {
        static subsystem_t power_sub{"power"};
        cluster = boost::add_vertex (g);
        node = boost::add_vertex (g);
        core0 = boost::add_vertex (g);
        core1 = boost::add_vertex (g);
        pdu = boost::add_vertex (g);
        if (power_first)
            link (pdu, node, power_sub);
        link (cluster, node, containment_sub);
        link (node, core0, containment_sub);
        link (node, core1, containment_sub);
        if (!power_first)
            link (pdu, node, power_sub);
    }

    void link (vtx_t src, vtx_t tgt, subsystem_t s)
    {
        edg_t e = boost::add_edge (src, tgt, g).first;
        g[e].subsystem = s;
    }

    bool counts_are (int64_t c_cluster, int64_t c_node, int64_t c_core0, int64_t c_core1)
    {
        return g[cluster].schedule.subtree_allocations == c_cluster
               && g[node].schedule.subtree_allocations == c_node
               && g[core0].schedule.subtree_allocations == c_core0
               && g[core1].schedule.subtree_allocations == c_core1
               && g[pdu].schedule.subtree_allocations == 0;
    }
};

static void test_set_and_erase (bool power_first)
{
    const char *order = power_first ? "power edge first" : "power edge last";
    graph_fixture_t f{power_first};

    ok (f.counts_are (0, 0, 0, 0), "%s: a new graph has no allocation", order);

    set_allocation (f.g, f.core1, 1, 10);
    ok (f.counts_are (1, 1, 0, 1),
        "%s: an allocation on a core counts on the core and its ancestors",
        order);

    set_allocation (f.g, f.core1, 1, 11);
    ok (f.g[f.core1].schedule.allocations.at (1) == 11 && f.counts_are (1, 1, 0, 1),
        "%s: a new span for the same job replaces the span and does not count again",
        order);

    set_allocation (f.g, f.core1, 2, 12);
    set_allocation (f.g, f.node, 3, 13);
    ok (f.counts_are (3, 3, 0, 2), "%s: each entry counts one time on each ancestor", order);

    erase_allocation (f.g, f.core1, 1);
    ok (f.counts_are (2, 2, 0, 1),
        "%s: an erase subtracts one on the vertex and its ancestors",
        order);

    erase_allocation (f.g, f.core1, 1);
    ok (f.counts_are (2, 2, 0, 1), "%s: an erase of an absent entry changes nothing", order);

    clear_allocations (f.g, f.core1);
    ok (f.g[f.core1].schedule.allocations.empty () && f.counts_are (1, 1, 0, 0),
        "%s: a clear subtracts the size of the map",
        order);

    clear_allocations (f.g, f.core1);
    ok (f.counts_are (1, 1, 0, 0), "%s: a clear of an empty map changes nothing", order);

    erase_allocation (f.g, f.node, 3);
    ok (f.counts_are (0, 0, 0, 0), "%s: the counts return to zero after the last erase", order);
}

static void test_copy_and_compare ()
{
    graph_fixture_t f;

    set_allocation (f.g, f.core0, 1, 10);
    schedule_t copy = f.g[f.node].schedule;
    ok (copy.subtree_allocations == 1, "the copy constructor copies the count");

    schedule_t assigned;
    assigned = f.g[f.node].schedule;
    ok (assigned.subtree_allocations == 1, "the assignment copies the count");

    ok (copy == f.g[f.node].schedule, "a schedule equals its copy");
    copy.subtree_allocations = 2;
    ok (!(copy == f.g[f.node].schedule), "a schedule with another count is not equal");
}

int main (int argc, char *argv[])
{
    plan (NO_PLAN);
    test_set_and_erase (false);
    test_set_and_erase (true);
    test_copy_and_compare ();
    done_testing ();
    return 0;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
