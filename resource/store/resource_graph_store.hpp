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
#include <unordered_set>
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
    // by_jobid maps a job to every vertex holding any state keyed by
    // that job (schedule.allocations/reservations and the idata
    // tags/x_spans/job2span entries), so that job removal can visit
    // exactly the job's vertices without a graph traversal, a rank
    // index, or a tag trail. The stored descriptors rely on vertices
    // never being deleted from the graph: shrink and subgraph removal
    // clear vertices and their metadata but never call remove_vertex,
    // so vecS descriptors are stable for the lifetime of the graph.
    std::map<int64_t, std::unordered_set<vtx_t>> by_jobid;
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

    /*! Record in by_jobid that vertex v holds state keyed by jobid.
     */
    void add_job_vertex (int64_t jobid, vtx_t v);

    /*! Erase vertex v from by_jobid[jobid]; drops the jobid entry when
     *  its vertex set becomes empty.
     */
    void remove_job_vertex (int64_t jobid, vtx_t v);

    /*! Return true if vertex v holds any state keyed by jobid
     *  (allocations, reservations, tags, exclusive-filter spans, or
     *  aggregate-filter spans).
     */
    bool vertex_has_job_state (const resource_graph_t &g, int64_t jobid, vtx_t v) const;

    /*! Verify the by_jobid index bidirectionally against the graph:
     *  every indexed (jobid, vertex) pair must have state on the
     *  vertex, and every vertex holding job-keyed state must be
     *  indexed. Intended for tests and debugging; O(V).
     */
    bool verify_job_index (const resource_graph_t &g) const;
};

/*! Resource graph data store.
 *  Adjacency_list graph, roots of this graph and various indexing.
 */
struct resource_graph_db_t {
    resource_graph_t resource_graph;
    resource_graph_metadata_t metadata;

    resource_graph_db_t () = default;
    resource_graph_db_t (const resource_graph_db_t &o);
    resource_graph_db_t &operator= (const resource_graph_db_t &o);

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
