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
#include <boost/flyweight.hpp>

using namespace Flux;
using namespace Flux::resource_model;


/********************************************************************************
 *                                                                              *
 *                   Public RV1EXEC Resource Reader Interface                   *
 *                                                                              *
 ********************************************************************************/

int resource_reader_rv1exec_t::properties_t::insert (
                                                 const std::string &res_type,
                                                 const std::string &property)
{
    if (m_properties.find (res_type) == m_properties.end ()) {
        auto res = m_properties.insert (
                       std::pair<std::string,
                                 std::map<std::string,
                                          std::string>> (
                                              res_type,
                                              std::map<std::string,
                                                       std::string> ()));
        if (!res.second) {
            errno = ENOMEM;
            return -1;
        }
    }
    auto res2 = m_properties[res_type].insert (std::pair<std::string,
                                                         std::string> (property,
                                                                       ""));
    if (!res2.second) {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

bool resource_reader_rv1exec_t::properties_t::exist (
                                                  const std::string &res_type)
{
    return (m_properties.find (res_type) != m_properties.end ())? true : false;
}


int resource_reader_rv1exec_t::properties_t::copy (
                                   const std::string &res_type,
                                   std::map<std::string,
                                            std::string> &properties)
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
                                             vtx_t parent, int64_t id,
                                             const std::string &subsys,
                                             const std::string &type,
                                             const std::string &basename,
                                             const std::string &name,
                                             const std::map<std::string,
                                                            std::string> &props,
                                             int size, int rank)
{
    planner_t *plan = nullptr;
    planner_t *x_checker = nullptr;
    boost::flyweight<std::string> fly_subsystem(subsys);

    if ( !(plan = planner_new (0, INT64_MAX, size, type.c_str ())))
        return boost::graph_traits<resource_graph_t>::null_vertex ();

    if ( !(x_checker = planner_new (0, INT64_MAX,
                                    X_CHECKER_NJOBS, X_CHECKER_JOBS_STR)))
        return boost::graph_traits<resource_graph_t>::null_vertex ();

    vtx_t v = boost::add_vertex (g);

    // Set properties of the new vertex
    bool is_root = false;
    if (parent == boost::graph_traits<resource_graph_t>::null_vertex ())
        is_root = true;

    std::string istr = (id != -1)? std::to_string (id) : "";
    std::string prefix =  is_root ? "" : g[parent].paths[subsys];

    g[v].type = type;
    g[v].basename = basename;
    g[v].size = size;
    g[v].uniq_id = v;
    g[v].rank = rank;
    g[v].schedule.plans = plan;
    g[v].idata.x_checker = x_checker;
    g[v].id = id;
    g[v].name = (name != "")? name : basename + istr;
    g[v].paths[subsys] = prefix + "/" + g[v].name;
    g[v].idata.member_of[fly_subsystem] = "*";
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
                                             edg_t e, vtx_t src, vtx_t dst)
{
    // add this edge to by_outedges metadata
    auto iter = m.by_outedges.find (src);
    if (iter == m.by_outedges.end ()) {
        auto ret = m.by_outedges.insert (
                         std::make_pair (
                             src,
                             std::map<std::pair<uint64_t, int64_t>, edg_t,
                                      std::greater<
                                               std::pair<uint64_t,
                                                         int64_t>>> ()));
        if (!ret.second) {
            errno = ENOMEM;
            m_err_msg += __FUNCTION__;
            m_err_msg += "error creating out-edge metadata map: "
                              + g[src].name + " -> " + g[dst].name + "; ";
            return -1;
        }
        iter = m.by_outedges.find (src);
    }
    std::pair<uint64_t, int64_t> key = std::make_pair (
                                                g[e].idata.get_weight (),
                                                g[dst].uniq_id);
    auto ret = iter->second.insert (std::make_pair (key, e));
    if (!ret.second) {
        errno = ENOMEM;
        m_err_msg += __FUNCTION__;
        m_err_msg += "error inserting an edge to out-edge metadata map: "
                          + g[src].name + " -> " + g[dst].name + "; ";
        return -1;
    }
    return 0;
}

int resource_reader_rv1exec_t::add_edges (resource_graph_t &g,
                                          resource_graph_metadata_t &m,
                                          vtx_t src, vtx_t dst,
                                          const std::string &subsys,
                                          const std::string &relation,
                                          const std::string &rev_relation)
{
    edg_t e;
    bool inserted;
    boost::flyweight<std::string> fly_subsystem(subsys);

    tie (e, inserted) = add_edge (src, dst, g);
    if (!inserted) {
        errno = ENOMEM;
        goto error;
    }
    g[e].idata.member_of[fly_subsystem] = relation;
    g[e].name[subsys] = relation;
    if (add_metadata (g, m, e, src, dst) < 0)
       goto error;

    tie (e, inserted) = add_edge (dst, src, g);
    if (!inserted) {
        errno = ENOMEM;
        goto error;
    }
    g[e].idata.member_of[fly_subsystem] = rev_relation;
    g[e].name[subsys] = rev_relation;
    if (add_metadata (g, m, e, dst, src) < 0)
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
    if ( (v = add_vertex (g, m,
                          boost::graph_traits<resource_graph_t>::null_vertex (),
                          0, "containment", "cluster", "cluster", "", p, 1, -1))
         == boost::graph_traits<resource_graph_t>::null_vertex ())
        return -1;

    m.roots.emplace ("containment", v);
    m.v_rt_edges.emplace ("containment", relation_infra_t ());
    return 0;
}

int resource_reader_rv1exec_t::build_rmap (json_t *rlite,
                                           std::map<unsigned, unsigned> &rmap)
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
    if ( !(ids = idset_create (0, IDSET_FLAG_AUTOGROW)))
        goto error;

    // Create a global rank set that combines all ranks from R_lite array
    json_array_foreach (rlite, index, entry) {
        if (json_unpack (entry, "{s:s}", "rank", &ranks) < 0) {
            errno = EINVAL;
            goto error;
        }
        if ( !(r_ids = idset_decode (ranks)))
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
                                           std::map<unsigned,
                                                    properties_t> &pmap)
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
        if ( !(ranks = idset_decode (json_string_value (val)))) {
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
                auto res = pmap.insert (
                                    std::pair<unsigned,
                                              properties_t> (rank,
                                                             properties_t ()));
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
                                             std::map<unsigned,
                                                      properties_t> &pmap)
{
    int rc = -1;
    unsigned id;
    struct idset *ids = nullptr;

    if (!resource_type || !resource_ids) {
        errno = EINVAL;
        goto ret;
    }
    if ( !(ids = idset_decode (resource_ids)))
        goto ret;

    id = idset_first (ids);
    while (id != IDSET_INVALID_ID) {
        edg_t e;
        vtx_t v;
        std::string name = resource_type + std::to_string (id);
        std::map<std::string, std::string> p;
        if (pmap.find (rank) != pmap.end ()) {
            if (pmap[rank].exist (resource_type)) {
                if (pmap[rank].copy (resource_type, p) < 0)
                    goto ret;
            }
        }
        v = add_vertex (g, m, parent, id,
                        "containment", resource_type,
                        resource_type, name, p, 1, rank);
        if (v == boost::graph_traits<resource_graph_t>::null_vertex ())
            goto ret;
        if (add_edges (g, m, parent, v, "containment", "contains", "in") < 0)
            goto ret;

        id = idset_next (ids, id);
    }

    rc = 0;
ret:
    idset_destroy (ids);
    return rc;
}


int resource_reader_rv1exec_t::unpack_children (resource_graph_t &g,
                                                resource_graph_metadata_t &m,
                                                vtx_t parent,
                                                json_t *children,
                                                unsigned rank,
                                                std::map<unsigned,
                                                         properties_t> &pmap)
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
        if (unpack_child (g, m, parent, res_type, ids_str, rank, pmap) < 0)
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
                                            std::map<unsigned,
                                                     properties_t> &pmap)
{
    edg_t e;
    vtx_t v;
    int64_t iden;
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

    if ( !(hostname = hostlist_nth (hlist, static_cast<int> (rmap[rank]))))
        goto error;
    if (get_hostname_suffix (hostname, iden) < 0
        || get_host_basename (hostname, basename) < 0) {
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

    // Create and add a node vertex and link with cluster vertex
    v = add_vertex (g, m, parent, iden, "containment",
                    "node", basename, hostname, properties, 1, rank);
    if (v == boost::graph_traits<resource_graph_t>::null_vertex ())
        goto error;
    if (add_edges (g, m, parent, v, "containment", "contains", "in") < 0)
        goto error;
    // Unpack children node-local resources
    if (unpack_children (g, m, v, children, rank, pmap) < 0)
        goto error;

    return 0;

error:
    return -1;
}

int resource_reader_rv1exec_t::unpack_rlite_entry (resource_graph_t &g,
                                                   resource_graph_metadata_t &m,
                                                   vtx_t parent,
                                                   json_t *entry,
                                                   struct hostlist *hlist,
                                                   std::map<unsigned,
                                                            unsigned> &rmap,
                                                   std::map<unsigned,
                                                            properties_t> &pmap)
{
    int rc = -1;
    unsigned rank;
    json_t *children = nullptr;
    const char *ranks = nullptr;
    struct idset *r_ids = nullptr;

    if (!entry || !hlist) {
        errno = EINVAL;
        goto ret;
    }

    if (json_unpack (entry, "{s:s s:o}",
                                "rank", &ranks,
                                "children", &children) < 0) {
        errno = EINVAL;
        goto ret;
    }

    if ( !(r_ids = idset_decode (ranks)))
        goto ret;

    rank = idset_first (r_ids);
    while (rank != IDSET_INVALID_ID) {
        if (unpack_rank (g, m, parent, rank, children, hlist, rmap, pmap) < 0)
            goto ret;

        rank = idset_next (r_ids, rank);
    }

    rc = 0;
ret:
    idset_destroy (r_ids);
    return rc;
}

int resource_reader_rv1exec_t::unpack_rlite (resource_graph_t &g,
                                             resource_graph_metadata_t &m,
                                             json_t *rlite,
                                             struct hostlist *hlist,
                                             std::map<unsigned, unsigned> &rmap,
                                             std::map<unsigned,
                                                      properties_t> &pmap)
{
    size_t index;
    vtx_t cluster_vtx;
    json_t *entry = nullptr;

    if (!rlite || !hlist) {
        errno = EINVAL;
        goto error;
    }

    if (m.roots.find (flux_subsystem_containment) == m.roots.end ()) {
        errno = ENOENT;
        goto error;
    }

    cluster_vtx = m.roots[flux_subsystem_containment];
    json_array_foreach (rlite, index, entry) {
        if (unpack_rlite_entry (g, m, cluster_vtx,
                                entry, hlist, rmap, pmap) < 0)
            goto error;
    }
    return 0;

error:
    return -1;
}

int resource_reader_rv1exec_t::unpack_internal (resource_graph_t &g,
                                                resource_graph_metadata_t &m,
                                                json_t *rv1)
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

    if (json_unpack (rv1, "{s:i s:{s:o s:o s?o}}",
                              "version", &version,
                              "execution",
                                  "R_lite", &rlite,
                                  "nodelist", &nodelist,
                                  "properties", &properties) < 0) {
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
    if ( !(hlist = hostlist_create ()))
        goto ret;

    // Encode all nodes in nodelist array into a hostlist object.
    json_array_foreach (nodelist, index, val) {
        const char *hlist_str = nullptr;
        if (!json_is_string (val)) {
            errno = EINVAL;
            goto ret;
        }
        if ( !(hlist_str = json_string_value (val))) {
            errno = EINVAL;
            goto ret;
        }
        if (hostlist_append (hlist, hlist_str) < 0)
            goto ret;
    }
    if (unpack_rlite (g, m, rlite, hlist, rmap, pmap) < 0)
        goto ret;

    rc = 0;

ret:
    hostlist_destroy (hlist);
    return rc;
}


/********************************************************************************
 *                                                                              *
 *                   Public RV1EXEC Resource Reader Interface                   *
 *                                                                              *
 ********************************************************************************/

resource_reader_rv1exec_t::~resource_reader_rv1exec_t ()
{

}

int resource_reader_rv1exec_t::unpack (resource_graph_t &g,
                                       resource_graph_metadata_t &m,
                                       const std::string &str, int rank)
{
    int rc = -1;
    json_error_t error;
    json_t *rv1 = nullptr;
    int saved_errno;

    if (str == "") {
        errno = EINVAL;
        goto ret;
    }
    if (add_cluster_vertex (g, m) < 0)
        goto ret;

    if ( !(rv1 = json_loads (str.c_str (), 0, &error))) {
        errno = ENOMEM;
        goto ret;
    }
    rc = unpack_internal (g, m, rv1);

ret:
    saved_errno = errno;
    json_decref (rv1);
    errno = saved_errno;
    return rc;
}

int resource_reader_rv1exec_t::unpack_at (resource_graph_t &g,
                                          resource_graph_metadata_t &m,
                                          vtx_t &vtx,
                                          const std::string &str, int rank)
{
    errno = ENOTSUP;
    return -1;
}

int resource_reader_rv1exec_t::remove_subgraph (resource_graph_t &g,
                                                resource_graph_metadata_t &m,
                                                const std::string &path)
{
   errno = ENOTSUP; // RV1Exec reader does not support remove
   return -1;
}

int resource_reader_rv1exec_t::update (resource_graph_t &g,
                                       resource_graph_metadata_t &m,
                                       const std::string &str, int64_t jobid,
                                       int64_t at, uint64_t dur, bool rsv,
                                       uint64_t token)
{
    errno = ENOTSUP; // RV1Exec reader currently does not support update
    return -1;
}

bool resource_reader_rv1exec_t::is_allowlist_supported ()
{
    return false;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
