/*****************************************************************************\
 * Copyright 2022 Lawrence Livermore National Security, LLC
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
#include <flux/idset.h>
}

#include <map>
#include <string>
#include <jansson.h>

#include "resource/readers/resource_reader_rv1exec.hpp"
#include "resource/planner/c/planner.h"

using namespace Flux;
using namespace Flux::resource_model;

////////////////////////////////////////////////////////////////////////////////
// Public RV1EXEC Resource Reader Interface
////////////////////////////////////////////////////////////////////////////////

int resource_reader_rv1exec_t::properties_t::insert (const std::string &res_type,
                                                     const std::string &property)
{
    if (m_properties.find (res_type) == m_properties.end ()) {
        auto res = m_properties.insert (
            std::pair<std::string,
                      std::map<std::string, std::string>> (res_type,
                                                           std::map<std::string, std::string> ()));
        if (!res.second) {
            errno = ENOMEM;
            return -1;
        }
    }
    auto res2 = m_properties[res_type].insert (std::pair<std::string, std::string> (property, ""));
    if (!res2.second) {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

bool resource_reader_rv1exec_t::properties_t::exist (const std::string &res_type)
{
    return (m_properties.find (res_type) != m_properties.end ()) ? true : false;
}

int resource_reader_rv1exec_t::properties_t::copy (const std::string &res_type,
                                                   std::map<std::string, std::string> &properties)
{
    int rc = -1;
    if (m_properties.find (res_type) != m_properties.end ()) {
        properties = m_properties[res_type];
        rc = 0;
    }
    return rc;
}

vtx_t resource_reader_rv1exec_t::add_vertex (resource_graph_t &g,
                                             resource_graph_metadata_t &m,
                                             vtx_t parent,
                                             int64_t id,
                                             subsystem_t subsys,
                                             resource_type_t type,
                                             const std::string &basename,
                                             const std::string &name,
                                             const std::map<std::string, std::string> &props,
                                             int size,
                                             int rank)
{
    planner_t *plan = nullptr;
    planner_t *x_checker = nullptr;

    if (!(plan = planner_new (0, INT64_MAX, size, type.c_str ())))
        return boost::graph_traits<resource_graph_t>::null_vertex ();

    if (!(x_checker = planner_new (0, INT64_MAX, X_CHECKER_NJOBS, X_CHECKER_JOBS_STR)))
        return boost::graph_traits<resource_graph_t>::null_vertex ();

    vtx_t v = boost::add_vertex (g);

    // Set properties of the new vertex
    bool is_root = false;
    if (parent == boost::graph_traits<resource_graph_t>::null_vertex ())
        is_root = true;

    std::string istr = (id != -1) ? std::to_string (id) : "";
    std::string prefix = is_root ? "" : g[parent].paths[subsys];

    g[v].type = type;
    g[v].basename = basename;
    g[v].size = size;
    g[v].uniq_id = v;
    g[v].rank = rank;
    g[v].schedule.plans = plan;
    g[v].idata.x_checker = x_checker;
    g[v].id = id;
    g[v].name = (name != "") ? name : basename + istr;
    g[v].paths[subsys] = prefix + "/" + g[v].name;
    g[v].idata.member_of[subsys] = "*";
    g[v].status = resource_pool_t::status_t::UP;
    g[v].properties = props;

    // Indexing for fast look-up
    m.by_path[g[v].paths[subsys]].push_back (v);
    m.by_type[g[v].type].push_back (v);
    m.by_name[g[v].name].push_back (v);
    m.by_rank[rank].push_back (v);

    return v;
}

int resource_reader_rv1exec_t::add_metadata (resource_graph_t &g,
                                             resource_graph_metadata_t &m,
                                             edg_t e,
                                             vtx_t src,
                                             vtx_t dst)
{
    // add this edge to by_outedges metadata
    auto iter = m.by_outedges.find (src);
    if (iter == m.by_outedges.end ()) {
        auto ret = m.by_outedges.insert (
            std::make_pair (src,
                            std::map<std::pair<uint64_t, int64_t>,
                                     edg_t,
                                     std::greater<std::pair<uint64_t, int64_t>>> ()));
        if (!ret.second) {
            errno = ENOMEM;
            m_err_msg += __FUNCTION__;
            m_err_msg += "error creating out-edge metadata map: " + g[src].name + " -> "
                         + g[dst].name + "; ";
            return -1;
        }
        iter = m.by_outedges.find (src);
    }
    std::pair<uint64_t, int64_t> key = std::make_pair (g[e].idata.get_weight (), g[dst].uniq_id);
    auto ret = iter->second.insert (std::make_pair (key, e));
    if (!ret.second) {
        errno = ENOMEM;
        m_err_msg += __FUNCTION__;
        m_err_msg += "error inserting an edge to out-edge metadata map: " + g[src].name + " -> "
                     + g[dst].name + "; ";
        return -1;
    }
    return 0;
}

int resource_reader_rv1exec_t::add_edges (resource_graph_t &g,
                                          resource_graph_metadata_t &m,
                                          vtx_t src,
                                          vtx_t dst,
                                          subsystem_t subsys,
                                          const std::string &relation,
                                          const std::string &rev_relation)
{
    edg_t e;
    bool inserted;

    tie (e, inserted) = add_edge (src, dst, g);
    if (!inserted) {
        errno = ENOMEM;
        goto error;
    }
    g[e].idata.member_of[subsys] = true;
    g[e].subsystem = subsys;
    if (add_metadata (g, m, e, src, dst) < 0)
        goto error;

    return 0;

error:
    return -1;
}

int resource_reader_rv1exec_t::add_cluster_vertex (resource_graph_t &g,
                                                   resource_graph_metadata_t &m)
{
    vtx_t v;
    const std::map<std::string, std::string> p;
    if ((v = add_vertex (g,
                         m,
                         boost::graph_traits<resource_graph_t>::null_vertex (),
                         0,
                         containment_sub,
                         cluster_rt,
                         "cluster",
                         "",
                         p,
                         1,
                         -1))
        == boost::graph_traits<resource_graph_t>::null_vertex ())
        return -1;

    m.roots.emplace ("containment", v);
    m.v_rt_edges.emplace ("containment", relation_infra_t ());
    return 0;
}

vtx_t resource_reader_rv1exec_t::find_vertex (resource_graph_t &g,
                                              resource_graph_metadata_t &m,
                                              vtx_t parent,
                                              int64_t id,
                                              subsystem_t subsys,
                                              resource_type_t type,
                                              const std::string &basename,
                                              const std::string &name,
                                              int size,
                                              int rank)
{
    bool is_root = false;
    std::string path = "";
    std::string vtx_name = "";
    vtx_t null_vtx = boost::graph_traits<resource_graph_t>::null_vertex ();

    // Get properties of the vertex
    if (parent == boost::graph_traits<resource_graph_t>::null_vertex ())
        is_root = true;

    std::string idstr = (id != -1) ? std::to_string (id) : "";
    std::string prefix = is_root ? "" : g[parent].paths[subsys];
    vtx_name = (name != "") ? name : basename + idstr;
    path = prefix + "/" + vtx_name;

    // Search graph metadata for vertex
    const auto &vtx_iter = m.by_path.find (path);
    // Not found; return null_vertex
    if (vtx_iter == m.by_path.end ())
        return null_vtx;
    // Found in by_path
    for (const vtx_t &v : vtx_iter->second) {
        if (g[v].rank == rank && g[v].id == id && g[v].size == size && g[v].type == type) {
            return v;
        }
    }

    return null_vtx;
}

int resource_reader_rv1exec_t::update_vertex (resource_graph_t &g,
                                              vtx_t vtx,
                                              updater_data &update_data)
{
    int rc = -1;
    int64_t span = -1;
    int64_t avail = -1;
    planner_t *plans = NULL;

    // Check and update plan
    if ((plans = g[vtx].schedule.plans) == NULL) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": plan for " + g[vtx].name + " is null.\n";
        goto error;
    }
    if ((avail = planner_avail_resources_during (plans, update_data.at, update_data.duration))
        == -1) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": planner_avail_resource_during return -1 for ";
        m_err_msg += g[vtx].name + ".\n";
        goto error;
    }
    if (avail < g[vtx].size) {
        // if g[v] has already been allocated/reserved, this is an error
        m_err_msg += __FUNCTION__;
        m_err_msg += ": " + g[vtx].name + " is unavailable.\n";
        goto error;
    }
    // Update the vertex plan here (not in traverser code).
    // Traverser update () will handle aggregate filters and
    // exclusivity checking filter.
    // Can't update the rank-level (node) vertex yet- we
    // don't know if all its children are allocated.
    // Note this is a hard-coded option. Support for more flexible
    // types may require extending rv1exec.
    if (g[vtx].type == node_rt)
        return 0;
    // Name is anything besides node
    if ((span = planner_add_span (plans,
                                  update_data.at,
                                  update_data.duration,
                                  static_cast<const uint64_t> (g[vtx].size)))
        == -1) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": can't add span into " + g[vtx].name + ".\n";
        goto error;
    }

    if (update_data.reserved)
        g[vtx].schedule.reservations[update_data.jobid] = span;
    else
        g[vtx].schedule.allocations[update_data.jobid] = span;

    update_data.updated_vertices[g[vtx].rank].push_back (vtx);

    rc = 0;

error:
    return rc;
}

int resource_reader_rv1exec_t::undo_vertices (resource_graph_t &g, updater_data &update_data)
{
    int rc = -1;
    int64_t span = -1;
    planner_t *plans = NULL;

    for (auto &[rank, vertices] : update_data.updated_vertices) {
        for (const vtx_t &vtx : vertices) {
            // Check plan
            if ((plans = g[vtx].schedule.plans) == NULL) {
                errno = EINVAL;
                m_err_msg += __FUNCTION__;
                m_err_msg += ": plan for " + g[vtx].name + " is null.\n";
                goto error;
            }
            // Remove job tags
            if (update_data.reserved) {
                span = g[vtx].schedule.reservations.at (update_data.jobid);
                g[vtx].schedule.reservations.erase (update_data.jobid);
            } else {
                span = g[vtx].schedule.allocations.at (update_data.jobid);
                g[vtx].schedule.allocations.erase (update_data.jobid);
            }
            // Remove the span.
            if (planner_rem_span (plans, span) == -1) {
                m_err_msg += __FUNCTION__;
                m_err_msg += ": can't remove span from " + g[vtx].name + ".\n";
                goto error;
            }
        }
    }

    rc = 0;

error:
    return rc;
}

int resource_reader_rv1exec_t::update_edges (resource_graph_t &g,
                                             resource_graph_metadata_t &m,
                                             vtx_t src,
                                             vtx_t dst,
                                             updater_data &update_data)
{
    edg_t e;
    int rc = -1;
    bool found = false;
    boost::graph_traits<resource_graph_t>::out_edge_iterator ei, ei_end;

    boost::tie (ei, ei_end) = boost::out_edges (src, g);
    for (; ei != ei_end; ++ei) {
        if (boost::target (*ei, g) == dst) {
            e = *ei;
            found = true;
            break;
        }
    }
    if (!found) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": rv1exec edge not found in resource graph.\n";
        goto error;
    }
    g[e].idata.set_for_trav_update (g[dst].size, true, update_data.token);

    return 0;

error:
    return -1;
}

int resource_reader_rv1exec_t::update_exclusivity (resource_graph_t &g,
                                                   resource_graph_metadata_t &m,
                                                   vtx_t vtx,
                                                   updater_data &update_data)
{
    // idata tag and exclusive checker update
    int64_t span = -1;
    planner_t *plans = NULL;

    const auto &rank_ex = update_data.updated_vertices.find (g[vtx].rank);
    if (rank_ex == update_data.updated_vertices.end ()) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": rank not found in agfilters map.\n";
        return -1;
    }
    // This check enforces a rigid constraint on rank equivalence
    // between graph initialization and rank in rv1exec string.
    const auto &by_rank = m.by_rank.find (g[vtx].rank);
    if (by_rank == m.by_rank.end ()) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": rank not found in by_rank graph map.\n";
        return -1;
    }

    // If all child vertices allocated, allocate this vertex.
    // Subtract one since the current node hasn't been added to the
    // updated_vertices map.
    if (rank_ex->second.size () != (by_rank->second.size () - 1))
        return 0;
    // Counts indicate exclusive
    if ((plans = g[vtx].schedule.plans) == NULL) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": plan for " + g[vtx].name + " is null.\n";
        return -1;
    }
    // Update the vertex plan here (not in traverser code).
    // Traverser update () will handle aggregate filters and
    // exclusivity checking filter.
    if ((span = planner_add_span (plans,
                                  update_data.at,
                                  update_data.duration,
                                  static_cast<const uint64_t> (g[vtx].size)))
        == -1) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": can't add span into " + g[vtx].name + ".\n";
        return -1;
    }
    // Add job tags
    if (update_data.reserved)
        g[vtx].schedule.reservations[update_data.jobid] = span;
    else
        g[vtx].schedule.allocations[update_data.jobid] = span;
    // Add to the updated vertices vector to undo upon error.
    update_data.updated_vertices[g[vtx].rank].push_back (vtx);

    return 0;
}

vtx_t resource_reader_rv1exec_t::add_or_update (resource_graph_t &g,
                                                resource_graph_metadata_t &m,
                                                vtx_t parent,
                                                int64_t id,
                                                subsystem_t subsys,
                                                resource_type_t type,
                                                const std::string &basename,
                                                const std::string &name,
                                                int size,
                                                int rank,
                                                std::map<std::string, std::string> &properties,
                                                updater_data &update_data)
{
    vtx_t vtx;
    vtx_t null_vtx = boost::graph_traits<resource_graph_t>::null_vertex ();

    if (!update_data.update) {
        vtx = find_vertex (g, m, parent, id, subsys, type, basename, name, size, rank);
        // Shouldn't be found
        if (vtx != boost::graph_traits<resource_graph_t>::null_vertex ()) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": found duplicate vertex in graph for ";
            m_err_msg += name + ".\n";
            return null_vtx;
        }
        // Add resources
        vtx = add_vertex (g, m, parent, id, subsys, type, basename, name, properties, size, rank);
        if (vtx == boost::graph_traits<resource_graph_t>::null_vertex ()) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": failed to add vertex for ";
            m_err_msg += name + ".\n";
            return null_vtx;
        }
        if (add_edges (g, m, parent, vtx, subsys, "contains", "in") < 0) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": failed to add edges for ";
            m_err_msg += name + ".\n";
            return null_vtx;
        }
    } else {
        // Update resources
        vtx = find_vertex (g, m, parent, id, subsys, type, basename, name, size, rank);
        // Not found
        if (vtx == boost::graph_traits<resource_graph_t>::null_vertex ()) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": couldn't find vertex in graph for ";
            m_err_msg += name + ".\n";
            return null_vtx;
        }
        if (update_vertex (g, vtx, update_data) == -1)
            return null_vtx;
        // Must be the containment subsystem
        if (update_edges (g, m, parent, vtx, update_data) == -1)
            return null_vtx;
    }

    return vtx;
}

int resource_reader_rv1exec_t::build_rmap (json_t *rlite, std::map<unsigned, unsigned> &rmap)
{
    int i;
    size_t index;
    unsigned rank;
    json_t *entry = nullptr;
    const char *ranks = nullptr;
    struct idset *ids = nullptr;
    struct idset *r_ids = nullptr;

    if (!rlite) {
        errno = EINVAL;
        goto error;
    }
    if (!(ids = idset_create (0, IDSET_FLAG_AUTOGROW)))
        goto error;

    // Create a global rank set that combines all ranks from R_lite array
    json_array_foreach (rlite, index, entry) {
        if (json_unpack (entry, "{s:s}", "rank", &ranks) < 0) {
            errno = EINVAL;
            goto error;
        }
        if (!(r_ids = idset_decode (ranks)))
            goto error;

        rank = idset_first (r_ids);
        while (rank != IDSET_INVALID_ID) {
            if (idset_set (ids, rank) < 0)
                goto error;

            rank = idset_next (r_ids, rank);
        }
        idset_destroy (r_ids);
    }

    // A rank map mapping mononically increasing ranks to consecutive IDs from 0
    i = 0;
    rank = idset_first (ids);
    while (rank != IDSET_INVALID_ID) {
        if (rmap.find (rank) != rmap.end ()) {
            errno = EEXIST;
            m_err_msg += __FUNCTION__;
            m_err_msg += "rmap has no rank=" + std::to_string (rank) + "; ";
            goto error;
        }

        auto res = rmap.insert (std::pair<unsigned, unsigned> (rank, i));
        if (!res.second) {
            errno = ENOMEM;
            goto error;
        }

        rank = idset_next (ids, rank);
        i++;
    }
    idset_destroy (ids);
    return 0;

error:
    idset_destroy (r_ids);
    idset_destroy (ids);
    return -1;
}

int resource_reader_rv1exec_t::build_pmap (json_t *properties,
                                           std::map<unsigned, properties_t> &pmap)
{
    const char *key = nullptr;
    json_t *val = nullptr;
    struct idset *ranks = nullptr;

    json_object_foreach (properties, key, val) {
        unsigned rank = IDSET_INVALID_ID;

        if (!json_is_string (val)) {
            errno = EINVAL;
            goto error;
        }
        if (!(ranks = idset_decode (json_string_value (val)))) {
            errno = ENOMEM;
            goto error;
        }

        rank = idset_first (ranks);
        while (rank != IDSET_INVALID_ID) {
            std::string prop = key;

            // When fine-grained resource property tagging use case comes along,
            // the following code can be extended: fetching @resource_type and
            // add the property to the corresponding resource_type vertices.
            // For now, we just add the property to compute node vertices.
            if (pmap.find (rank) != pmap.end ()) {
                if (pmap[rank].insert ("node", key) < 0)
                    goto error;
            } else {
                auto res = pmap.insert (std::pair<unsigned, properties_t> (rank, properties_t ()));
                if (!res.second) {
                    errno = ENOMEM;
                    goto error;
                }
                if (pmap[rank].insert ("node", key) < 0)
                    goto error;
            }
            rank = idset_next (ranks, rank);
        }
        idset_destroy (ranks);
        ranks = nullptr;
    }
    return 0;

error:
    idset_destroy (ranks);
    return -1;
}

int resource_reader_rv1exec_t::unpack_child (resource_graph_t &g,
                                             resource_graph_metadata_t &m,
                                             vtx_t parent,
                                             const char *resource_type,
                                             const char *resource_ids,
                                             unsigned rank,
                                             std::map<unsigned, properties_t> &pmap,
                                             updater_data &update_data)
{
    int rc = -1;
    unsigned id;
    struct idset *ids = nullptr;

    if (!resource_type || !resource_ids) {
        errno = EINVAL;
        goto error;
    }
    if (!(ids = idset_decode (resource_ids)))
        goto error;

    id = idset_first (ids);
    while (id != IDSET_INVALID_ID) {
        edg_t e;
        vtx_t vtx;
        std::string name = resource_type + std::to_string (id);
        std::map<std::string, std::string> properties;
        if (pmap.find (rank) != pmap.end ()) {
            if (pmap[rank].exist (resource_type)) {
                if (pmap[rank].copy (resource_type, properties) < 0)
                    goto error;
            }
        }
        // Returns the added or updated vertex; null_vertex on error.
        vtx = add_or_update (g,
                             m,
                             parent,
                             id,
                             containment_sub,
                             resource_type_t{resource_type},
                             resource_type,
                             name,
                             1,
                             rank,
                             properties,
                             update_data);
        if (vtx == boost::graph_traits<resource_graph_t>::null_vertex ()) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": failed unpacking child for ";
            m_err_msg += name + ".\n";
            goto error;
        }

        id = idset_next (ids, id);
    }

    rc = 0;
error:
    idset_destroy (ids);
    return rc;
}

int resource_reader_rv1exec_t::unpack_children (resource_graph_t &g,
                                                resource_graph_metadata_t &m,
                                                vtx_t parent,
                                                json_t *children,
                                                unsigned rank,
                                                std::map<unsigned, properties_t> &pmap,
                                                updater_data &update_data)
{
    json_t *res_ids = nullptr;
    const char *res_type = nullptr;

    if (!children) {
        errno = EINVAL;
        goto error;
    }

    json_object_foreach (children, res_type, res_ids) {
        if (!json_is_string (res_ids)) {
            errno = EINVAL;
            goto error;
        }
        const char *ids_str = json_string_value (res_ids);
        if (unpack_child (g, m, parent, res_type, ids_str, rank, pmap, update_data) < 0)
            goto error;
    }
    return 0;

error:
    return -1;
}

int resource_reader_rv1exec_t::unpack_rank (resource_graph_t &g,
                                            resource_graph_metadata_t &m,
                                            vtx_t parent,
                                            unsigned rank,
                                            json_t *children,
                                            struct hostlist *hlist,
                                            std::map<unsigned, unsigned> &rmap,
                                            std::map<unsigned, properties_t> &pmap,
                                            updater_data &update_data)
{
    edg_t e;
    vtx_t vtx;
    int64_t id;
    const char *hostname = nullptr;
    std::string basename;
    std::map<std::string, std::string> properties;

    if (!children || !hlist) {
        errno = EINVAL;
        goto error;
    }
    if (rmap.find (rank) == rmap.end ()) {
        errno = EINVAL;
        goto error;
    }

    if (!(hostname = hostlist_nth (hlist, static_cast<int> (rmap[rank]))))
        goto error;
    if (get_hostname_suffix (hostname, id) < 0 || get_host_basename (hostname, basename) < 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": error splitting hostname=";
        m_err_msg += hostname + std::string ("; ");
        goto error;
    }
    if (pmap.find (rank) != pmap.end ()) {
        if (pmap[rank].exist ("node")) {
            if (pmap[rank].copy ("node", properties) < 0)
                goto error;
        }
    }
    // Returns the added or updated vertex; null_vtertex on error.
    vtx = add_or_update (g,
                         m,
                         parent,
                         id,
                         containment_sub,
                         node_rt,
                         basename,
                         hostname,
                         1,
                         rank,
                         properties,
                         update_data);
    if (vtx == boost::graph_traits<resource_graph_t>::null_vertex ()) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": failed unpacking rank for ";
        m_err_msg += std::string (hostname) + ".\n";
        goto error;
    }
    // Unpack children node-local resources
    if (unpack_children (g, m, vtx, children, rank, pmap, update_data) < 0)
        goto error;

    if (update_data.update) {
        // Update the rank's planner if all children allocated
        if (update_exclusivity (g, m, vtx, update_data) == -1) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": exclusive filter update failed for ";
            m_err_msg += std::string (hostname) + ".\n";
            goto error;
        }
    }

    return 0;

error:
    return -1;
}

int resource_reader_rv1exec_t::unpack_rlite_entry (resource_graph_t &g,
                                                   resource_graph_metadata_t &m,
                                                   vtx_t parent,
                                                   json_t *entry,
                                                   struct hostlist *hlist,
                                                   std::map<unsigned, unsigned> &rmap,
                                                   std::map<unsigned, properties_t> &pmap,
                                                   updater_data &update_data)
{
    int rc = -1;
    unsigned rank;
    json_t *children = nullptr;
    const char *ranks = nullptr;
    struct idset *r_ids = nullptr;

    if (!entry || !hlist) {
        errno = EINVAL;
        goto error;
    }

    if (json_unpack (entry, "{s:s s:o}", "rank", &ranks, "children", &children) < 0) {
        errno = EINVAL;
        goto error;
    }

    if (!(r_ids = idset_decode (ranks)))
        goto error;

    rank = idset_first (r_ids);
    while (rank != IDSET_INVALID_ID) {
        if (unpack_rank (g, m, parent, rank, children, hlist, rmap, pmap, update_data) < 0)
            goto error;

        rank = idset_next (r_ids, rank);
    }

    rc = 0;
error:
    idset_destroy (r_ids);
    return rc;
}

int resource_reader_rv1exec_t::unpack_rlite (resource_graph_t &g,
                                             resource_graph_metadata_t &m,
                                             json_t *rlite,
                                             struct hostlist *hlist,
                                             std::map<unsigned, unsigned> &rmap,
                                             std::map<unsigned, properties_t> &pmap,
                                             updater_data &update_data)
{
    size_t index;
    vtx_t cluster_vtx;
    json_t *entry = nullptr;

    if (!rlite || !hlist) {
        errno = EINVAL;
        goto error;
    }

    if (m.roots.find (containment_sub) == m.roots.end ()) {
        errno = ENOENT;
        goto error;
    }

    cluster_vtx = m.roots[containment_sub];
    // Set the cluster "needs" and make the update shared access to the cluster
    m.v_rt_edges[containment_sub].set_for_trav_update (g[cluster_vtx].size,
                                                       false,
                                                       update_data.token);
    json_array_foreach (rlite, index, entry) {
        if (unpack_rlite_entry (g, m, cluster_vtx, entry, hlist, rmap, pmap, update_data) < 0)
            goto error;
    }
    return 0;

error:
    return -1;
}

int resource_reader_rv1exec_t::unpack_internal (resource_graph_t &g,
                                                resource_graph_metadata_t &m,
                                                json_t *rv1,
                                                updater_data &update_data)
{
    int rc = -1;
    int version;
    size_t index;
    json_t *val = nullptr;
    json_t *rlite = nullptr;
    json_t *nodelist = nullptr;
    json_t *properties = nullptr;
    struct hostlist *hlist = nullptr;
    std::map<unsigned, unsigned> rmap;
    std::map<unsigned, properties_t> pmap;

    if (json_unpack (rv1,
                     "{s:i s:{s:o s:o s?o}}",
                     "version",
                     &version,
                     "execution",
                     "R_lite",
                     &rlite,
                     "nodelist",
                     &nodelist,
                     "properties",
                     &properties)
        < 0) {
        errno = EINVAL;
        goto ret;
    }
    if (version != 1) {
        errno = EINVAL;
        goto ret;
    }
    // Create rank-to-0-based-index mapping.
    if (build_rmap (rlite, rmap) < 0)
        goto ret;
    if (build_pmap (properties, pmap) < 0)
        goto ret;
    if (!(hlist = hostlist_create ()))
        goto ret;

    // Encode all nodes in nodelist array into a hostlist object.
    json_array_foreach (nodelist, index, val) {
        const char *hlist_str = nullptr;
        if (!json_is_string (val)) {
            errno = EINVAL;
            goto ret;
        }
        if (!(hlist_str = json_string_value (val))) {
            errno = EINVAL;
            goto ret;
        }
        if (hostlist_append (hlist, hlist_str) < 0)
            goto ret;
    }
    if (unpack_rlite (g, m, rlite, hlist, rmap, pmap, update_data) < 0)
        goto ret;

    rc = 0;

ret:
    hostlist_destroy (hlist);
    return rc;
}

int resource_reader_rv1exec_t::partial_cancel_internal (resource_graph_t &g,
                                                        resource_graph_metadata_t &m,
                                                        modify_data_t &mod_data,
                                                        json_t *rv1)
{
    int rc = -1;
    int version;
    int64_t rank;
    size_t index;
    json_t *rlite = nullptr;
    json_t *entry = nullptr;
    const char *ranks = nullptr;
    struct idset *r_ids = nullptr;
    size_t len;

    // Implementing cancellation of rank subgraph
    // will require further parsing of nodelist,
    // children, and rank
    if (json_unpack (rv1, "{s:i s:{s:o}}", "version", &version, "execution", "R_lite", &rlite)
        < 0) {
        errno = EINVAL;
        goto error;
    }
    if (version != 1) {
        errno = EINVAL;
        goto error;
    }
    if (!(r_ids = idset_create (0, IDSET_FLAG_AUTOGROW)))
        goto error;
    json_array_foreach (rlite, index, entry) {
        if (json_unpack (entry, "{s:s%}", "rank", &ranks, &len) < 0) {
            errno = EINVAL;
            goto error;
        }
        if (idset_decode_add (r_ids, ranks, len, NULL) < 0)
            goto error;
    }
    rank = idset_first (r_ids);
    while (rank != IDSET_INVALID_ID) {
        mod_data.ranks_removed.insert (rank);
        rank = idset_next (r_ids, rank);
    }
    rc = 0;
error:
    idset_destroy (r_ids);
    return rc;
}

////////////////////////////////////////////////////////////////////////////////
// Public RV1EXEC Resource Reader Interface
////////////////////////////////////////////////////////////////////////////////

resource_reader_rv1exec_t::~resource_reader_rv1exec_t ()
{
}

int resource_reader_rv1exec_t::unpack (resource_graph_t &g,
                                       resource_graph_metadata_t &m,
                                       const std::string &str,
                                       int rank)
{
    int rc = -1;
    json_error_t error;
    json_t *rv1 = nullptr;
    int saved_errno;
    updater_data null_data;
    // Indicate adding, not updating
    null_data.update = false;

    if (str == "") {
        errno = EINVAL;
        goto ret;
    }
    if (add_cluster_vertex (g, m) < 0)
        goto ret;

    if (!(rv1 = json_loads (str.c_str (), 0, &error))) {
        errno = ENOMEM;
        goto ret;
    }

    rc = unpack_internal (g, m, rv1, null_data);

ret:
    saved_errno = errno;
    json_decref (rv1);
    errno = saved_errno;
    return rc;
}

int resource_reader_rv1exec_t::unpack_at (resource_graph_t &g,
                                          resource_graph_metadata_t &m,
                                          vtx_t &vtx,
                                          const std::string &str,
                                          int rank)
{
    errno = ENOTSUP;
    return -1;
}

int resource_reader_rv1exec_t::update (resource_graph_t &g,
                                       resource_graph_metadata_t &m,
                                       const std::string &R,
                                       int64_t jobid,
                                       int64_t at,
                                       uint64_t dur,
                                       bool rsv,
                                       uint64_t token)
{
    int rc = -1;
    json_error_t error;
    json_t *rv1 = nullptr;
    int saved_errno;
    updater_data update_data;

    if (R == "") {
        errno = EINVAL;
        goto ret;
    }

    if (!(rv1 = json_loads (R.c_str (), 0, &error))) {
        errno = ENOMEM;
        goto ret;
    }

    update_data.update = true;
    update_data.jobid = jobid;
    update_data.at = at;
    update_data.duration = dur;
    update_data.reserved = rsv;
    update_data.token = token;

    if ((rc = unpack_internal (g, m, rv1, update_data)) == -1) {
        undo_vertices (g, update_data);
    }

ret:
    saved_errno = errno;
    json_decref (rv1);
    errno = saved_errno;
    return rc;
}

bool resource_reader_rv1exec_t::is_allowlist_supported ()
{
    return false;
}

int resource_reader_rv1exec_t::partial_cancel (resource_graph_t &g,
                                               resource_graph_metadata_t &m,
                                               modify_data_t &mod_data,
                                               const std::string &R,
                                               int64_t jobid)
{
    int rc = -1;
    json_error_t error;
    json_t *rv1 = nullptr;
    int saved_errno;

    if (R == "") {
        errno = EINVAL;
        goto ret;
    }

    if (!(rv1 = json_loads (R.c_str (), 0, &error))) {
        errno = ENOMEM;
        goto ret;
    }

    rc = partial_cancel_internal (g, m, mod_data, rv1);

ret:
    saved_errno = errno;
    json_decref (rv1);
    errno = saved_errno;
    return rc;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
