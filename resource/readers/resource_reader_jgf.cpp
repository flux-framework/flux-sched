/*****************************************************************************\
 *  Copyright (c) 2019 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
 *  LLNL-CODE-658032 All rights reserved.
 *
 *  This file is part of the Flux resource manager framework.
 *  For details, see https://github.com/flux-framework.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license, or (at your option)
 *  any later version.
 *
 *  Flux is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *  See also:  http://www.gnu.org/licenses/
 \*****************************************************************************/

#include <map>
#include <unistd.h>
#include <jansson.h>
#include "resource/readers/resource_reader_jgf.hpp"
#include "resource/store/resource_graph_store.hpp"
#include "resource/planner/planner.h"

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

using namespace Flux;
using namespace Flux::resource_model;

struct fetch_helper_t {
    int64_t id = 0;
    int64_t rank = 0;
    int64_t size = 0;
    int64_t uniq_id = 0;
    int exclusive = 0;
    resource_pool_t::status_t status = resource_pool_t::status_t::UP;
    const char *type = NULL;
    const char *name = NULL;
    const char *unit = NULL;
    const char *basename = NULL;
    const char *vertex_id= NULL;
    std::map<std::string, std::string> properties;
    std::map<std::string, std::string> paths;
};

struct vmap_val_t {
    vtx_t v;
    std::map<std::string, bool> is_roots;
    unsigned int needs;
    unsigned int exclusive;
};

bool operator== (const std::map<std::string, std::string> lhs,
                 const std::map<std::string, std::string> rhs)
{
    return (lhs.size () == rhs.size ())
            && (std::equal (lhs.begin (), lhs.end (), rhs.begin ()));
}

bool operator== (const resource_pool_t &r, const fetch_helper_t &f)
{
    return (r.type == f.type
            && r.basename == f.basename
            && r.size == static_cast<unsigned int> (f.size)
            && r.rank == static_cast<int> (f.rank)
            && r.id == f.id
            && r.name == f.name
            && r.properties == f.properties
            && r.paths == f.paths);
}

bool operator!= (const resource_pool_t &r, const fetch_helper_t &f)
{
    return !(r == f);
}

std::string diff (const resource_pool_t &r, const fetch_helper_t &f)
{
    std::stringstream sstream;
    if (r.type != f.type)
        sstream << "type=(" << r.type << ", " << f.type << ")";
    if (r.basename != f.basename)
        sstream << " basename=(" << r.basename << ", " << f.basename << ")";
    if (r.size != f.size)
        sstream << " size=(" << r.size << ", " << f.size << ")";
    if (r.id != f.id)
        sstream << " id=(" << r.id << ", " << f.id << ")";
    if (r.name != f.name)
        sstream << " name=(" << r.name << ", " << f.name << ")";
    if (r.properties != f.properties) {
        sstream << " properties=(";
        for (auto &kv : r.properties)
            sstream << kv.first << "=" << kv.second << " ";
        sstream << ", ";
        for (auto &kv : f.properties)
            sstream << kv.first << "=" << kv.second << " ";
        sstream << ")";
    }
    if (r.paths != f.paths) {
        sstream << " paths=(";
        for (auto &kv : r.paths)
            sstream << kv.first << "=" << kv.second << " ";
        sstream << ", ";
        for (auto &kv : f.paths)
            sstream << kv.first << "=" << kv.second << " ";
        sstream << ")";
    }
    return sstream.str ();
}

/********************************************************************************
 *                                                                              *
 *                        Private JGF Resource Reader                           *
 *                                                                              *
 ********************************************************************************/

int resource_reader_jgf_t::fetch_jgf (const std::string &str, json_t **jgf_p,
                                      json_t **nodes_p, json_t **edges_p)
{
    int rc = -1;
    json_t *graph = NULL;
    json_error_t json_err;

    if ( (*jgf_p = json_loads (str.c_str (), 0, &json_err)) == NULL) {
        errno = EPROTO;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": json_loads returned an error: ";
        m_err_msg += std::string (json_err.text) + std::string (": ");
        m_err_msg += std::string (json_err.source) + std::string ("@")
                     + std::to_string (json_err.line) + std::string (":")
                     + std::to_string (json_err.column) + ".\n";
        goto done;
    }
    if ( (graph = json_object_get (*jgf_p, "graph" )) == NULL) {
        errno = EPROTO;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": JGF does not contain a required key (graph).\n";
        goto done;
    }
    if ( (*nodes_p = json_object_get (graph, "nodes" )) == NULL) {
        errno = EPROTO;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": JGF does not contain a required key (nodes).\n";
        goto done;
    }
    if ( (*edges_p = json_object_get (graph, "edges" )) == NULL) {
        errno = EPROTO;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": JGF does not contain a required key (edges).\n";
        goto done;
    }
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::unpack_vtx (json_t *element, fetch_helper_t &f)
{
    int rc = -1;
    json_t *p = NULL;
    json_t *value = NULL;
    json_t *metadata = NULL;
    const char *key = NULL;

    if ( (json_unpack (element, "{ s:s }", "id", &f.vertex_id) < 0)) {
        errno = EPROTO;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": JGF vertex id key is not found in a node.\n";
        goto done;
    }
    if ( (metadata = json_object_get (element, "metadata")) == NULL) {
        errno = EPROTO;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": key (metadata) is not found in an JGF node for ";
        m_err_msg += std::string (f.vertex_id) + ".\n";
        goto done;
    }
    if ( (json_unpack (metadata, "{ s:s s:s s:s s:I s:I s:I s?:i s:b s:s s:I }",
                                 "type", &f.type, "basename", &f.basename,
                                 "name", &f.name, "id", &f.id,
                                 "uniq_id", &f.uniq_id, "rank", &f.rank,
                                 "status", &f.status, "exclusive", &f.exclusive,
                                 "unit", &f.unit, "size", &f.size)) < 0) {
        errno = EPROTO;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": malformed metadata in an JGF node for ";
        m_err_msg += std::string (f.vertex_id) + "\n";
        goto done;
    }
    if ( (p = json_object_get (metadata, "paths")) == NULL) {
        errno = EPROTO;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": key (paths) does not exist in an JGF node for ";
        m_err_msg += std::string (f.vertex_id) + ".\n";
        goto done;
    }
    json_object_foreach (p, key, value) {
        f.paths[std::string (key)] = std::string (json_string_value (value));
    }

    p = json_object_get (metadata, "properties");
    json_object_foreach (p, key, value) {
        f.properties[std::string (key)]
            = std::string (json_string_value (value));
    }
    rc = 0;

done:
    return rc;
}

vtx_t resource_reader_jgf_t::create_vtx (resource_graph_t &g,
                                         const fetch_helper_t &fetcher)
{
    planner_t *plans = NULL;
    planner_t *x_checker = NULL;
    vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();

    if ( !(plans = planner_new (0, INT64_MAX, fetcher.size, fetcher.type))) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": planner_new returned NULL.\n";
        goto done;
    }
    if ( !(x_checker = planner_new (0, INT64_MAX,
                                    X_CHECKER_NJOBS, X_CHECKER_JOBS_STR))) {
        m_err_msg += __FUNCTION__;
        m_err_msg += "planner_new for x_checker returned NULL.\n";
        goto done;
    }

    v = boost::add_vertex (g);
    g[v].type = fetcher.type;
    g[v].basename = fetcher.basename;
    g[v].size = fetcher.size;
    g[v].uniq_id = fetcher.uniq_id;
    g[v].rank = fetcher.rank;
    g[v].status = fetcher.status;
    g[v].id = fetcher.id;
    g[v].name = fetcher.name;
    g[v].properties = fetcher.properties;
    g[v].paths = fetcher.paths;
    g[v].schedule.plans = plans;
    g[v].idata.x_checker = x_checker;
    for (auto kv : g[v].paths)
        g[v].idata.member_of[kv.first] = "*";

done:
    return v;
}

bool resource_reader_jgf_t::is_root (const std::string &path)
{
    return (std::count (path.begin (), path.end (), '/') == 1);
}

int resource_reader_jgf_t::check_root (vtx_t v, resource_graph_t &g,
                                       std::map<std::string, bool> &is_roots)
{
    int rc = -1;
    std::pair<std::map<std::string, bool>::iterator, bool> ptr;
    for (auto kv : g[v].paths) {
        if (is_root (kv.second)) {
            ptr = is_roots.emplace (kv.first, true);
            if (!ptr.second)
                goto done;
        }
    }
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::add_graph_metadata (vtx_t v,
                                               resource_graph_t &g,
                                               resource_graph_metadata_t &m)
{
    int rc = -1;
    std::pair<std::map<std::string, vtx_t>::iterator, bool> ptr;

    for (auto kv : g[v].paths) {
        if (is_root (kv.second)) {
            ptr = m.roots.emplace (kv.first, v);
            if (!ptr.second)
                goto done;
        }
        m.by_path[kv.second] = v;
    }
    m.by_type[g[v].type].push_back (v);
    m.by_name[g[v].name].push_back (v);
    m.by_rank[g[v].rank].push_back (v);
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::add_vtx (resource_graph_t &g,
                                    resource_graph_metadata_t &m,
                                    std::map<std::string, vmap_val_t> &vmap,
                                    const fetch_helper_t &fetcher)
{
    int rc = -1;
    bool root = false;
    std::map<std::string, bool> root_checks;
    std::pair<std::map<std::string, vmap_val_t>::iterator, bool> ptr;
    vtx_t nullvtx = boost::graph_traits<resource_graph_t>::null_vertex ();
    vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();

    if (vmap.find (fetcher.vertex_id) != vmap.end ()) {
        errno = EPROTO;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": found duplicate JGF node id for ";
        m_err_msg += std::string (fetcher.vertex_id) + ".\n";
        goto done;
    }
    if ( (v = create_vtx (g, fetcher)) == nullvtx)
        goto done;
    if ( (rc = check_root (v, g, root_checks)) == -1)
        goto done;
    if ( (rc = add_graph_metadata (v, g, m)) == -1)
        goto done;

    ptr = vmap.emplace (std::string (fetcher.vertex_id),
                        vmap_val_t{v, root_checks,
                            static_cast<unsigned int> (fetcher.size),
                            static_cast<unsigned int> (fetcher.exclusive)});
    if (!ptr.second) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": can't insert into vmap for ";
        m_err_msg += std::string (fetcher.vertex_id) + ".\n";
        goto done;
    }
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::find_vtx (resource_graph_t &g,
                                     resource_graph_metadata_t &m,
                                     std::map<std::string, vmap_val_t> &vmap,
                                     const fetch_helper_t &fetcher,
                                     vtx_t &v)
{
    int rc = -1;
    vtx_t nullvtx = boost::graph_traits<resource_graph_t>::null_vertex ();
    v = nullvtx;

    if (vmap.find (fetcher.vertex_id) != vmap.end ()) {
        errno = EPROTO;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": found duplicate JGF node id for ";
        m_err_msg += std::string (fetcher.vertex_id) + ".\n";
        goto done;
    }

    for (auto &kv : fetcher.paths) {
        try {
            vtx_t u = m.by_path.at (kv.second);
            if (v != nullvtx && u != v) {
                errno = EPROTO;
                m_err_msg += __FUNCTION__;
                m_err_msg += ": inconsistent input vertex for " + kv.second;
                m_err_msg += " (id=" + std::string (fetcher.vertex_id) + ").\n";
                m_err_msg += std::to_string (u) + " != " + std::to_string (v) + ".\n";
                goto done;
            }
            v = u;
        } catch (std::out_of_range &e) {
            errno = ENOENT;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": inconsistent input vertex: nonexistent path (";
            m_err_msg += kv.second + ") " + fetcher.vertex_id + ".\n";
            v = nullvtx;
            goto done;
        }
    }

    if (g[v] != fetcher) {
        errno = EPROTO;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": inconsistent input vertex for ";
        m_err_msg += std::string (fetcher.vertex_id) + ".\n";
        m_err_msg += diff (g[v], fetcher) + ".\n";
        goto done;
    }
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::update_vtx_plan (vtx_t v, resource_graph_t &g,
                                            const fetch_helper_t &fetcher,
                                            uint64_t jobid, int64_t at,
                                            uint64_t dur, bool rsv)
{
    int rc = -1;
    int64_t span = -1;
    int64_t avail = -1;
    planner_t *plans = NULL;

    if ( (plans = g[v].schedule.plans) == NULL) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": plan for " + g[v].name + " is null.\n";
        goto done;
    }
    if ( (avail = planner_avail_resources_during (plans, at, dur)) == -1) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": planner_avail_resource_during return -1 for ";
        m_err_msg + g[v].name + ".\n";
        goto done;
    }

    if (fetcher.exclusive) {
        // Update the vertex plan here (not in traverser code) so vertices
        // that the traverser won't walk still get their plans updated.
        if ( (span = planner_add_span (plans, at, dur,
                         static_cast<const uint64_t> (g[v].size))) == -1) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": can't add span into " + g[v].name + ".\n";
            goto done;
        }
        if (rsv)
            g[v].schedule.reservations[jobid] = span;
        else
            g[v].schedule.allocations[jobid] = span;
    } else {
        if (avail < g[v].size) {
            // if g[v] has already been allocated/reserved, this is an error
            m_err_msg += __FUNCTION__;
            m_err_msg += ": " + g[v].name + " is unavailable.\n";
            goto done;
        }
    }
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::update_vtx (resource_graph_t &g,
                                       resource_graph_metadata_t &m,
                                       std::map<std::string, vmap_val_t> &vmap,
                                       const fetch_helper_t &fetcher,
                                       uint64_t jobid, int64_t at,
                                       uint64_t dur, bool rsv)
{
    int rc = -1;
    int64_t span = -1;
    planner_t *plans = NULL;
    std::map<std::string, bool> root_checks;
    vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();
    std::pair<std::map<std::string, vmap_val_t>::iterator, bool> ptr;

    if ( (rc = find_vtx (g, m, vmap, fetcher, v)) != 0)
        goto done;
    if ( (rc = check_root (v, g, root_checks)) != 0)
        goto done;

    ptr = vmap.emplace (std::string (fetcher.vertex_id),
                        vmap_val_t{v, root_checks,
                            static_cast<unsigned int> (fetcher.size),
                            static_cast<unsigned int> (fetcher.exclusive)});
    if (!ptr.second) {
        errno = EPROTO;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": can't insert into vmap.\n";
        rc = -1;
        goto done;
    }

    if ( (rc = update_vtx_plan (v, g, fetcher, jobid, at, dur, rsv)) != 0)
        goto done;

done:
    return rc;
}

int resource_reader_jgf_t::undo_vertices (resource_graph_t &g,
                                          std::map<std::string, vmap_val_t> &vmap,
                                          uint64_t jobid, bool rsv)
{
    int rc = 0;
    int rc2 = 0;
    int64_t span = -1;
    planner_t *plans = NULL;
    vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();

    for (auto &kv : vmap) {
        if (kv.second.exclusive != 1)
            continue;
        try {
            v = kv.second.v;
            if (rsv) {
                span = g[v].schedule.reservations.at (jobid);
                g[v].schedule.reservations.erase (jobid);
            } else {
                span = g[v].schedule.allocations.at (jobid);
                g[v].schedule.allocations.erase (jobid);
            }

            plans = g[v].schedule.plans;
            if ( (rc2 = planner_rem_span (plans, span)) == -1) {
                m_err_msg += __FUNCTION__;
                m_err_msg += ": can't remove span from " + g[v].name + "\n.";
                m_err_msg += "resource graph state is likely corrupted.\n";
                rc += rc2;
                continue;
            }
        } catch (std::out_of_range &e) {
            continue;
        }
    }

    return (!rc)? 0 : -1;
}

int resource_reader_jgf_t::unpack_vertices (resource_graph_t &g,
                                            resource_graph_metadata_t &m,
                                            std::map<std::string,
                                                     vmap_val_t> &vmap,
                                            json_t *nodes)
{
    int rc = -1;
    unsigned int i = 0;
    fetch_helper_t fetcher;
    const char *vtx_id = NULL;
    vtx_t nullvtx = boost::graph_traits<resource_graph_t>::null_vertex ();

    for (i = 0; i < json_array_size (nodes); i++) {
        fetcher.properties.clear ();
        fetcher.paths.clear ();
        if (unpack_vtx (json_array_get (nodes, i), fetcher) != 0)
            goto done;
        if (add_vtx (g, m, vmap, fetcher) != 0)
            goto done;
    }
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::update_vertices (resource_graph_t &g,
                                            resource_graph_metadata_t &m,
                                            std::map<std::string,
                                                     vmap_val_t> &vmap,
                                            json_t *nodes, int64_t jobid,
                                            int64_t at, uint64_t dur,
                                            bool rsv)
{
    int rc = -1;
    unsigned int i = 0;
    fetch_helper_t fetcher;
    const char *vtx_id = NULL;

    for (i = 0; i < json_array_size (nodes); i++) {
        fetcher.properties.clear ();
        fetcher.paths.clear ();
        if ( (rc = unpack_vtx (json_array_get (nodes, i), fetcher)) != 0)
            goto done;
        if ( (rc = update_vtx (g, m, vmap, fetcher, jobid, at, dur, rsv)) != 0)
            goto done;
    }
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::unpack_edge (json_t *element,
                                        std::map<std::string,
                                                 vmap_val_t> &vmap,
                                        std::string &source,
                                        std::string &target,
                                        json_t **name)
{
    int rc = -1;
    json_t *metadata = NULL;
    const char *src = NULL;
    const char *tgt = NULL;

    if ( (json_unpack (element, "{ s:s s:s }", "source", &src,
                                               "target", &tgt)) < 0) {
        errno = EPROTO;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": encountered a malformed edge.\n";
        goto done;
    }
    source = src;
    target = tgt;
    if (vmap.find (source) == vmap.end ()
        || vmap.find (target) == vmap.end ()) {
        errno = EPROTO;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": source and/or target vertex not found";
        m_err_msg += source + std::string (" -> ") + target + ".\n";
        goto done;
    }
    if ( (metadata = json_object_get (element, "metadata")) == NULL) {
        errno = EPROTO;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": metadata key not found in an edge for ";
        m_err_msg += source + std::string (" -> ") + target + ".\n";
        goto done;
    }
    if ( (*name = json_object_get (metadata, "name")) == NULL) {
        errno = EPROTO;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": name key not found in edge metadata.\n";
        goto done;
    }
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::unpack_edges (resource_graph_t &g,
                                         resource_graph_metadata_t &m,
                                         std::map<std::string,
                                                  vmap_val_t> &vmap,
                                         json_t *edges)
{
    edg_t e;
    int rc = -1;
    unsigned int i = 0;
    json_t *name = NULL;
    json_t *element = NULL;
    json_t *value = NULL;
    bool inserted = false;
    const char *key = NULL;
    std::string source{};
    std::string target{};

    for (i = 0; i < json_array_size (edges); i++) {
        element = json_array_get (edges, i);
        if ( (unpack_edge (element, vmap, source, target, &name)) != 0)
            goto done;
        tie (e, inserted) = add_edge (vmap[source].v, vmap[target].v, g);
        if (inserted == false) {
            errno = EPROTO;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": couldn't add an edge to the graph for ";
            m_err_msg += source + std::string (" -> ") + target + ".\n";
            goto done;
        }
        json_object_foreach (name, key, value) {
            g[e].name[std::string (key)]
                = std::string (json_string_value (value));
            g[e].idata.member_of[std::string (key)]
                = std::string (json_string_value (value));
        }
    }
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::update_src_edge (resource_graph_t &g,
                                            resource_graph_metadata_t &m,
                                            std::map<std::string,
                                                     vmap_val_t> &vmap,
                                            std::string &source,
                                            uint64_t token)
{
    if (vmap[source].is_roots.empty ())
        return 0;

    for (auto &kv : vmap[source].is_roots)
        m.v_rt_edges[kv.first].set_for_trav_update (vmap[source].needs,
                                                    vmap[source].exclusive,
                                                    token);

    // This way, when a root vertex appears in multiple JGF edges
    // we only update the virtual in-edge into the root only once.
    vmap[source].is_roots.clear ();
    return 0;
}

int resource_reader_jgf_t::update_tgt_edge (resource_graph_t &g,
                                            resource_graph_metadata_t &m,
                                            std::map<std::string,
                                                     vmap_val_t> &vmap,
                                            std::string &source,
                                            std::string &target,
                                            uint64_t token)
{
    edg_t e;
    int rc = -1;
    bool found = false;
    boost::graph_traits<resource_graph_t>::out_edge_iterator ei, ei_end;
    boost::tie (ei, ei_end) = boost::out_edges (vmap[source].v, g);

    for (; ei != ei_end; ++ei) {
         if (boost::target (*ei, g) == vmap[target].v) {
              e = *ei;
              found = true;
              break;
         }
    }
    if (!found) {
        errno = EPROTO;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": JGF edge not found in resource graph.\n";
        goto done;
    }
    if (!(vmap[target].is_roots.empty ())) {
        errno = EPROTO;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": an edge into a root detected!\n";
        goto done;
    }
    g[e].idata.set_for_trav_update (vmap[target].needs,
                                    vmap[target].exclusive, token);
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::update_edges (resource_graph_t &g,
                                         resource_graph_metadata_t &m,
                                         std::map<std::string,
                                                  vmap_val_t> &vmap,
                                         json_t *edges, uint64_t token)
{
    edg_t e;
    int rc = -1;
    unsigned int i = 0;
    json_t *name = NULL;
    json_t *element = NULL;
    json_t *value = NULL;
    bool inserted = false;
    const char *key = NULL;
    std::string source{};
    std::string target{};

    for (i = 0; i < json_array_size (edges); i++) {
        element = json_array_get (edges, i);
        vtx_t src = boost::graph_traits<resource_graph_t>::null_vertex ();
        // We only check protocol errors in JGF edges in the following...
        if ( (rc = unpack_edge (element, vmap, source, target, &name)) != 0)
            goto done;
        if ( (rc = update_src_edge (g, m, vmap, source, token)) != 0)
            goto done;
        if ( (rc = update_tgt_edge (g, m, vmap, source, target, token)) != 0)
            goto done;
    }

done:
    return rc;
}


/********************************************************************************
 *                                                                              *
 *                   Public JGF Resource Reader Interface                       *
 *                                                                              *
 ********************************************************************************/

resource_reader_jgf_t::~resource_reader_jgf_t ()
{

}

int resource_reader_jgf_t::unpack (resource_graph_t &g,
                                   resource_graph_metadata_t &m,
                                   const std::string &str, int rank)

{
    int rc = -1;
    json_t *jgf = NULL;
    json_t *nodes = NULL;
    json_t *edges = NULL;
    std::map<std::string, vmap_val_t> vmap;

    if (rank != -1) {
        errno = ENOTSUP;
        m_err_msg += __FUNCTION__;
        m_err_msg += "rank != -1 unsupported for JGF unpack.\n";
        goto done;
    }
    if ( (rc = fetch_jgf (str, &jgf, &nodes, &edges)) != 0)
        goto done;
    if ( (rc = unpack_vertices (g, m, vmap, nodes)) != 0)
        goto done;
    if ( (rc = unpack_edges (g, m, vmap, edges)) != 0)
        goto done;

done:
    json_decref (jgf);
    return rc;
}

int resource_reader_jgf_t::unpack_at (resource_graph_t &g,
                                       resource_graph_metadata_t &m, vtx_t &vtx,
                                       const std::string &str, int rank)
{
    errno = ENOTSUP; // GRUG reader does not support unpack_at
    return -1;
}

int resource_reader_jgf_t::update (resource_graph_t &g,
                                   resource_graph_metadata_t &m,
                                   const std::string &str, int64_t jobid,
                                   int64_t at, uint64_t dur, bool rsv,
                                   uint64_t token)
{
    int rc = -1;
    json_t *jgf = NULL;
    json_t *nodes = NULL;
    json_t *edges = NULL;
    std::map<std::string, vmap_val_t> vmap;

    if (at < 0 || dur == 0) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": invalid time ("
                     + std::to_string (at) + ", "
                     + std::to_string (dur) + ").\n";
        goto done;
    }
    if ( (rc = fetch_jgf (str, &jgf, &nodes, &edges)) != 0)
        goto done;
    if ( (rc = update_vertices (g, m, vmap, nodes, jobid, at, dur, rsv)) != 0) {
        undo_vertices (g, vmap, jobid, rsv);
        goto done;
    }
    if ( (rc = update_edges (g, m, vmap, edges, token)) != 0)
        goto done;

done:
    json_decref (jgf);
    return rc;
}

bool resource_reader_jgf_t::is_whitelist_supported ()
{
    return false;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
