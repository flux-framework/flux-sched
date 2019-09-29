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

using namespace std;
using namespace Flux;
using namespace Flux::resource_model;

struct fetch_helper_t {
    int64_t id = 0;
    int64_t rank = 0;
    int64_t size = 0;
    int64_t uniq_id = 0;
    const char *type = NULL;
    const char *name = NULL;
    const char *unit = NULL;
    const char *basename = NULL;
    const char *vertex_id= NULL;
    std::map<std::string, std::string> properties;
    std::map<std::string, std::string> paths;
};


/********************************************************************************
 *                                                                              *
 *                        Private JGF Resource Reader                           *
 *                                                                              *
 ********************************************************************************/

int resource_reader_jgf_t::unpack_vtx (json_t *element, fetch_helper_t &f)
{
    int rc = -1;
    json_t *p = NULL;
    json_t *value = NULL;
    json_t *metadata = NULL;
    const char *key = NULL;

    if ( (json_unpack (element, "{ s:s }", "id", &f.vertex_id) < 0)) {
        errno = EPROTO;
        m_err_msg = "JGF vertex id key is not found in an node";
        goto done;
    }
    if ( (metadata = json_object_get (element, "metadata")) == NULL) {
        errno = EPROTO;
        m_err_msg = "key (metadata) is not found in an JGF node: ";
        m_err_msg += f.vertex_id;
        goto done;
    }
    // Note: Discard the exclusive field. A resource that is exlusively
    // allocated by the parent becomes a normal resource.
    if ( (json_unpack (metadata, "{ s:s s:s s:s s:I s:I s:I s:s s:I }",
                                 "type", &f.type, "basename", &f.basename,
                                 "name", &f.name, "id", &f.id,
                                 "uniq_id", &f.uniq_id, "rank", &f.rank,
                                 "unit", &f.unit, "size", &f.size)) < 0) {
        errno = EPROTO;
        m_err_msg = "malformed metadata in an JGF node: ";
        m_err_msg += f.vertex_id;
        goto done;
    }
    if ( (p = json_object_get (metadata, "paths")) == NULL) {
        errno = EPROTO;
        m_err_msg = "key (paths) does not exist in an JGF node: ";
        m_err_msg += f.vertex_id;
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

int resource_reader_jgf_t::add_vtx (resource_graph_t &g,
                                    resource_graph_metadata_t &m,
                                    std::map<std::string, vtx_t> &vmap,
                                    const fetch_helper_t &fetcher)
{
    int rc = -1;
    vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();

    if (vmap.find (fetcher.vertex_id) != vmap.end ()) {
        errno = EPROTO;
        m_err_msg = "found duplicate JGF node id: ";
        m_err_msg += fetcher.vertex_id;
        goto done;
    }

    v = boost::add_vertex (g);
    g[v].type = fetcher.type;
    g[v].basename = fetcher.basename;
    g[v].size = fetcher.size;
    g[v].uniq_id = fetcher.uniq_id;
    g[v].rank = fetcher.rank;
    if ( !(g[v].schedule.plans = planner_new (0, INT64_MAX,
                                              fetcher.size, fetcher.type))) {
        m_err_msg += "planner_new returned NULL ";
        goto done;
    }
    if ( !(g[v].schedule.x_checker = planner_new (0, INT64_MAX,
                                                  X_CHECKER_NJOBS,
                                                  X_CHECKER_JOBS_STR))) {
        m_err_msg += "planner_new returned NULL ";
        goto done;
    }
    g[v].id = fetcher.id;
    g[v].name = fetcher.basename + std::to_string (fetcher.id);
    g[v].properties = fetcher.properties;
    g[v].paths = fetcher.paths;

    for (auto kv : g[v].paths) {
        g[v].idata.member_of[kv.first] = "*";
        m.by_path[kv.second].push_back (v);
        if (std::count(kv.second.begin (), kv.second.end (), '/') == 1)
            m.roots[kv.first] = v;
    }
    m.by_type[g[v].type].push_back (v);
    m.by_name[g[v].name].push_back (v);

    vmap[std::string (fetcher.vertex_id)] = v;
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::unpack_vertices (resource_graph_t &g,
                                            resource_graph_metadata_t &m,
                                            std::map<std::string, vtx_t> &vmap,
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
        if (unpack_vtx (json_array_get (nodes, i), fetcher) < 0)
            goto done;
        if (add_vtx (g, m, vmap, fetcher) < 0)
            goto done;
    }
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::unpack_edg (json_t *element,
                                       std::map<std::string, vtx_t> &vmap,
                                       std::string &source, std::string &target,
                                       json_t **name)
{
    int rc = -1;
    json_t *metadata = NULL;
    const char *src = NULL;
    const char *tgt = NULL;

    if ( (json_unpack (element, "{ s:s s:s }", "source", &src,
                                               "target", &tgt)) < 0) {
        errno = EPROTO;
        m_err_msg = "encountered a malformed edge: ";
        goto done;
    }
    source = src;
    target = tgt;
    if (vmap.find (source) == vmap.end ()
        || vmap.find (target) == vmap.end ()) {
        errno = EPROTO;
        m_err_msg = "source and/or target vertex not found: ";
        m_err_msg += source + std::string (" -> ") + target;
        goto done;
    }
    if ( (metadata = json_object_get (element, "metadata")) == NULL) {
        errno = EPROTO;
        m_err_msg = "metadata key not found in an edge: ";
        m_err_msg += source + std::string (" -> ") + target;
        goto done;
    }
    if ( (*name = json_object_get (metadata, "name")) == NULL) {
        errno = EPROTO;
        m_err_msg = "name key not found in edge metadata";
        goto done;
    }
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::unpack_edges (resource_graph_t &g,
                                         resource_graph_metadata_t &m,
                                         std::map<std::string, vtx_t> &vmap,
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
        if ( (unpack_edg (element, vmap, source, target, &name)) < 0)
            goto done;
        tie (e, inserted) = add_edge (vmap[source], vmap[target], g);
        if (inserted == false) {
            errno = EPROTO;
            m_err_msg = "couldn't add an edge to the graph: ";
            m_err_msg += source + std::string (" -> ") + target;
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


/********************************************************************************
 *                                                                              *
 *                   Public JGF Resource Reader Interface                       *
 *                                                                              *
 ********************************************************************************/

int resource_reader_jgf_t::unpack (resource_graph_t &g,
                                   resource_graph_metadata_t &m,
                                   const std::string &str, int rank)

{
    int rc = -1;
    json_t *jgf = NULL;
    json_t *graph = NULL;
    json_t *nodes = NULL;
    json_t *edges = NULL;
    json_error_t json_err;
    std::map<std::string, vtx_t> vmap;

    if (rank != -1) {
        errno = ENOTSUP;
        m_err_msg = "rank != -1 unsupported for JGF unpack; ";
        goto done;
    }
    if ( (jgf = json_loads (str.c_str (), 0, &json_err)) == NULL) {
        errno = EPROTO;
        m_err_msg = "json_loads returned an error: ";
        m_err_msg += std::string (json_err.text) + std::string (": ");
        m_err_msg += std::string (json_err.source) + std::string ("@")
                     + std::to_string (json_err.line) + std::string (":")
                     + std::to_string (json_err.column);
        goto done;
    }
    if ( (graph = json_object_get (jgf, "graph" )) == NULL) {
        errno = EPROTO;
        m_err_msg = "JGF does not contain a required key (graph); ";
        goto done;
    }
    if ( (nodes = json_object_get (graph, "nodes" )) == NULL) {
        errno = EPROTO;
        m_err_msg = "JGF does not contain a required key (nodes); ";
        goto done;
    }
    if ( (edges = json_object_get (graph, "edges" )) == NULL) {
        errno = EPROTO;
        m_err_msg = "JGF does not contain a required key (edges); ";
        goto done;
    }
    if ( (rc = unpack_vertices (g, m, vmap, nodes)) < 0)
        goto done;
    if ( (rc = unpack_edges (g, m, vmap, edges)) < 0)
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

bool resource_reader_jgf_t::is_whitelist_supported ()
{
    return false;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
