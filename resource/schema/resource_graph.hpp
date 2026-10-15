/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef RESOURCE_GRAPH_HPP
#define RESOURCE_GRAPH_HPP

#include "resource/schema/resource_data.hpp"
#include <boost/graph/adjacency_list.hpp>

namespace Flux {
namespace resource_model {

enum class emit_format_t {
    GRAPHVIZ_DOT,
    GRAPH_ML,
};

using pinfra_t = pool_infra_t resource_pool_t::*;
using pname_t = std::string resource_t::*;
using rname_t = std::string resource_relation_t::*;
using rinfra_t = relation_infra_t resource_relation_t::*;
using resource_graph_t = boost::adjacency_list<boost::vecS,
                                               boost::vecS,
                                               boost::bidirectionalS,
                                               resource_pool_t,
                                               resource_relation_t,
                                               std::string>;

using vtx_infra_map_t = boost::property_map<resource_graph_t, pinfra_t>::type;
using edg_infra_map_t = boost::property_map<resource_graph_t, rinfra_t>::type;
using vtx_t = boost::graph_traits<resource_graph_t>::vertex_descriptor;
using edg_t = boost::graph_traits<resource_graph_t>::edge_descriptor;
using vtx_iterator_t = boost::graph_traits<resource_graph_t>::vertex_iterator;
using edg_iterator_t = boost::graph_traits<resource_graph_t>::edge_iterator;
using out_edg_iterator_t = boost::graph_traits<resource_graph_t>::out_edge_iterator;
using f_edg_infra_map_t = boost::property_map<resource_graph_t, rinfra_t>::type;
using f_vtx_infra_map_t = boost::property_map<resource_graph_t, pinfra_t>::type;
using f_res_name_map_t = boost::property_map<resource_graph_t, pname_t>::type;
using f_rel_name_map_t = boost::property_map<resource_graph_t, rname_t>::type;
using f_vtx_infra_map_t = boost::property_map<resource_graph_t, pinfra_t>::type;
using f_vtx_iterator_t = boost::graph_traits<resource_graph_t>::vertex_iterator;
using f_edg_iterator_t = boost::graph_traits<resource_graph_t>::edge_iterator;
using f_out_edg_iterator_t = boost::graph_traits<resource_graph_t>::out_edge_iterator;

/*! Add delta to schedule.subtree_allocations of v and of each ancestor of
 *  v in the containment subsystem. Call it once for each change of the
 *  size of schedule.allocations of v, with delta equal to the change.
 *  Precondition: the containment subsystem is a forest, so that each
 *  vertex has at most one containment parent and no cycle. The hwloc,
 *  rv1exec and grug readers build it that way, the jgf reader copies
 *  the edges of its input, and the traverser already depends on it.
 */
inline void add_subtree_allocations (resource_graph_t &g, vtx_t v, int64_t delta)
{
    if (delta == 0)
        return;
    for (;;) {
        g[v].schedule.subtree_allocations += delta;
        auto [ei, ei_end] = boost::in_edges (v, g);
        for (; ei != ei_end; ++ei)
            if (g[*ei].subsystem == containment_sub)
                break;
        if (ei == ei_end)
            return;
        v = boost::source (*ei, g);
    }
}

//! Set the allocation span of a job on v, and keep subtree_allocations exact.
inline void set_allocation (resource_graph_t &g, vtx_t v, int64_t jobid, int64_t span)
{
    if (g[v].schedule.allocations.insert_or_assign (jobid, span).second)
        add_subtree_allocations (g, v, 1);
}

//! Erase the allocation span of a job on v, and keep subtree_allocations exact.
inline void erase_allocation (resource_graph_t &g, vtx_t v, int64_t jobid)
{
    if (g[v].schedule.allocations.erase (jobid) != 0)
        add_subtree_allocations (g, v, -1);
}

//! Erase all allocation spans on v, and keep subtree_allocations exact.
inline void clear_allocations (resource_graph_t &g, vtx_t v)
{
    add_subtree_allocations (g, v, -static_cast<int64_t> (g[v].schedule.allocations.size ()));
    g[v].schedule.allocations.clear ();
}

}  // namespace resource_model
}  // namespace Flux

#endif  // RESOURCE_GRAPH_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
