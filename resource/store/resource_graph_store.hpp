/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef RESOURCE_GRAPH_STORE_HPP
#define RESOURCE_GRAPH_STORE_HPP

#include <string>
#include <memory>
#include <chrono>
#include "resource/schema/resource_graph.hpp"
#include "resource/config/system_defaults.hpp"

namespace Flux {
namespace resource_model {

class resource_reader_base_t;
struct resource_graph_db_t;

struct graph_duration_t {
    std::chrono::time_point<std::chrono::system_clock> graph_start =
        std::chrono::system_clock::from_time_t (0);
    std::chrono::time_point<std::chrono::system_clock> graph_end =
        std::chrono::system_clock::from_time_t (detail::SYSTEM_MAX_DURATION);
};

/*! Resource graph data metadata.
 *  Adjacency_list graph, roots of this graph and various indexing.
 */
struct resource_graph_metadata_t {
    std::map<subsystem_t, vtx_t> roots;
    std::map<subsystem_t, relation_infra_t> v_rt_edges;
    std::map<resource_type_t, std::vector<vtx_t>> by_type;
    std::map<std::string, std::vector<vtx_t>> by_name;
    std::map<int64_t, std::vector<vtx_t>> by_rank;
    std::map<std::string, std::vector<vtx_t>> by_path;
    // by_outedges enables graph traversing order to edge "weight"
    // E.g., the more available resources an edge point to, the heavier
    std::map<
        vtx_t,
        std::map<std::pair<uint64_t, int64_t>, edg_t, std::greater<std::pair<uint64_t, int64_t>>>>
        by_outedges;
    graph_duration_t graph_duration;
    int64_t nodes_up = 0;

    /*! Set the resource graph duration.
     *
     * \param g_duration  graph_duration_t of time_points used to set
     *                    the graph duration
     */
    void set_graph_duration (graph_duration_t &g_duration);
    void update_node_stats (int count, resource_pool_t::status_t status);
    void initialize_node_stats (resource_graph_t const &g);
};

/*! Resource graph data store.
 *  Adjacency_list graph, roots of this graph and various indexing.
 */
struct resource_graph_db_t {
    resource_graph_t resource_graph;
    resource_graph_metadata_t metadata;

    /*! Return true if s is known subsystem
     */
    bool known_subsystem (subsystem_t s);

    /*! Load str into the resource graph
     *
     * \param str    string containing a GRUG specification
     * \param reader resource reader base class object
     * \param rank   assign this rank to all the newly created resource vertices
     * \return       0 on success; non-zero integer on an error
     *                   ENOMEM: out of memory
     *                   EINVAL: invalid input or operation (e.g.
     *                               hwloc version or json string load error)
     *                   EPROTO: str violates the schema
     */
    int load (const std::string &str,
              std::shared_ptr<resource_reader_base_t> &reader,
              int rank = -1);

    /*! Load str into the resource graph and graft the top-level
     *  vertices to vtx_at.
     * \param str    string containing a GRUG specification
     * \param reader resource reader base class object
     * \param vtx_at parent vtx at which to graft the deserialized graph
     * \param rank   assign this rank to all the newly created resource vertices
     * \return       0 on success; non-zero integer on an error
     *                   ENOMEM: out of memory
     *                   EINVAL: invalid input or operation (e.g.
     *                               hwloc version or json string load error)
     *                   EPROTO: str violates the schema
     */
    int load (const std::string &str,
              std::shared_ptr<resource_reader_base_t> &reader,
              vtx_t &vtx_at,
              int rank = -1);
};

}  // namespace resource_model
}  // namespace Flux

#endif  // RESOURCE_GRAPH_STORE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
