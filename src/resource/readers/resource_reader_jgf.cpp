/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
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
}

#include <map>
#include <unordered_set>
#include <unistd.h>
#include <jansson.h>
#include "resource/readers/resource_reader_jgf.hpp"
#include "resource/store/resource_graph_store.hpp"
#include "resource/planner/c/planner.h"

using namespace Flux;
using namespace Flux::resource_model;

class fetch_remap_support_t {
public:
    int64_t get_remapped_id () const;
    int64_t get_remapped_rank () const;
    const std::string &get_remapped_name () const;
    void set_remapped_id (int64_t i);
    void set_remapped_rank (int64_t r);
    void set_remapped_name (const std::string &n);
    bool is_name_remapped () const;
    bool is_id_remapped () const;
    bool is_rank_remapped () const;
    void clear ();

private:
    int64_t m_remapped_id = -1;
    int64_t m_remapped_rank = -1;
    std::string m_remapped_name = "";
};

struct fetch_helper_t : public fetch_remap_support_t {
    const char *get_proper_name () const;
    int64_t get_proper_id () const;
    int64_t get_proper_rank () const;
    void scrub ();

    int64_t id = -1;
    int64_t rank = -1;
    int64_t size = -1;
    int64_t uniq_id = -1;
    int exclusive = -1;
    resource_pool_t::status_t status = resource_pool_t::status_t::UP;
    const char *type = NULL;
    const char *name = NULL;
    const char *unit = NULL;
    const char *basename = NULL;
    const char *vertex_id= NULL;
    std::map<std::string, std::string> properties;
    std::map<std::string, std::string> paths;
};

int64_t fetch_remap_support_t::get_remapped_id () const
{
    return m_remapped_id;
}

int64_t fetch_remap_support_t::get_remapped_rank () const
{
    return m_remapped_rank;
}

const std::string &fetch_remap_support_t::get_remapped_name () const
{
    return m_remapped_name;
}

void fetch_remap_support_t::set_remapped_id (int64_t i)
{
    m_remapped_id = i;
}

void fetch_remap_support_t::set_remapped_rank (int64_t r)
{
    m_remapped_rank = r;
}

void fetch_remap_support_t::set_remapped_name (const std::string &n)
{
    m_remapped_name = n;
}

bool fetch_remap_support_t::is_name_remapped () const
{
    return m_remapped_name != "";
}

bool fetch_remap_support_t::is_id_remapped () const
{
    return m_remapped_id != -1;
}

bool fetch_remap_support_t::is_rank_remapped () const
{
    return m_remapped_rank != -1;
}

void fetch_remap_support_t::clear ()
{
    m_remapped_id = -1;
    m_remapped_rank = -1;
    m_remapped_name = "";
}

const char *fetch_helper_t::get_proper_name () const
{
    return (is_name_remapped ())? get_remapped_name ().c_str () : name;
}

int64_t fetch_helper_t::get_proper_id () const
{
    return (is_id_remapped ())? get_remapped_id () : id;
}

int64_t fetch_helper_t::get_proper_rank () const
{
    return (is_rank_remapped ())? get_remapped_rank () : rank;
}

void fetch_helper_t::scrub ()
{
    id = -1;
    rank = -1;
    size = -1;
    uniq_id = -1;
    exclusive = -1;
    status = resource_pool_t::status_t::UP;
    type = NULL;
    name = NULL;
    unit = NULL;
    basename = NULL;
    vertex_id = NULL;
    properties.clear ();
    paths.clear ();
    clear ();
}


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
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": json_loads returned an error: ";
        m_err_msg += std::string (json_err.text) + std::string (": ");
        m_err_msg += std::string (json_err.source) + std::string ("@")
                     + std::to_string (json_err.line) + std::string (":")
                     + std::to_string (json_err.column) + ".\n";
        goto done;
    }
    if ( (graph = json_object_get (*jgf_p, "graph" )) == NULL) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": JGF does not contain a required key (graph).\n";
        goto done;
    }
    if ( (*nodes_p = json_object_get (graph, "nodes" )) == NULL) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": JGF does not contain a required key (nodes).\n";
        goto done;
    }
    if ( (*edges_p = json_object_get (graph, "edges" )) == NULL) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": JGF does not contain a required key (edges).\n";
        goto done;
    }
    rc = 0;

done:
    return rc;
}

/* JGF reader remaps execution target Ids for all resources
 * and certain info for core resources. (each core's id, name and paths).
 */
int resource_reader_jgf_t::unpack_and_remap_vtx (fetch_helper_t &f,
                                                 json_t *paths,
                                                 json_t *properties)
{
    json_t *value = NULL;
    const char *key = NULL;
    uint64_t remap_rank;
    uint64_t remap_id;

    if (namespace_remapper.query_exec_target (
                               static_cast<uint64_t> (f.rank),
                               remap_rank) < 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": error remapping rank id=";
        m_err_msg += std::to_string (f.rank) + ".\n";
        goto error;
    }
    if (remap_rank
            > static_cast<uint64_t> (std::numeric_limits<int64_t>::max ())) {
        errno = EOVERFLOW;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": remapped rank too large.\n";
        goto error;
    }
    f.set_remapped_rank (static_cast<int64_t> (remap_rank));

    if (std::string ("core") == f.type) {
        if (namespace_remapper.query (f.rank, "core", f.id, remap_id) < 0) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": error remapping core id=" + std::to_string (f.id);
            m_err_msg += " rank=" + std::to_string (f.rank) + ".\n";
            goto error;
        }
        if (remap_id
                > static_cast<uint64_t> (std::numeric_limits<int>::max ())) {
            errno = EOVERFLOW;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": remapped id too large.\n";
            goto error;
        }
        f.set_remapped_id (static_cast<int> (remap_id));
        f.set_remapped_name (f.basename + std::to_string (remap_id));

        json_object_foreach (paths, key, value) {
            std::string path = json_string_value (value);
            std::size_t sl = path.find_last_of ("/");
            if (sl == std::string::npos || path.substr (sl+1, 4) != "core") {
                errno = EINVAL;
                m_err_msg += __FUNCTION__;
                m_err_msg += ": malformed path for core id=";
                m_err_msg += std::to_string (f.id) + ".\n";
                goto error;
            }
            f.paths[std::string (key)] = path.substr (0, sl+1)
                                             + f.basename
                                             + std::to_string (remap_id);
        }
    } else {
        json_object_foreach (paths, key, value) {
            f.paths[std::string (key)] = json_string_value (value);
        }
    }

    json_object_foreach (properties, key, value) {
        f.properties[std::string (key)]
            = std::string (json_string_value (value));
    }
    return 0;
error:
    return -1;
}

int resource_reader_jgf_t::remap_aware_unpack_vtx (fetch_helper_t &f,
                                                   json_t *paths,
                                                   json_t *properties)
{
    json_t *value = NULL;
    const char *key = NULL;

    if (namespace_remapper.is_remapped () && f.rank != -1) {
        if (unpack_and_remap_vtx (f, paths, properties) < 0)
            return -1;
    } else {
        json_object_foreach (paths, key, value) {
            f.paths[std::string (key)] = std::string (json_string_value (value));
        }
        json_object_foreach (properties, key, value) {
            f.properties[std::string (key)]
                = std::string (json_string_value (value));
        }
    }
    return 0;
}

int resource_reader_jgf_t::fill_fetcher (json_t *element, fetch_helper_t &f,
                                         json_t **paths, json_t **properties)
{
    int rc = -1;
    json_t *p = NULL;
    json_t *metadata = NULL;

    if ( (json_unpack (element, "{ s:s }", "id", &f.vertex_id) < 0)) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": JGF vertex id key is not found in a node.\n";
        goto done;
    }
    if ( (metadata = json_object_get (element, "metadata")) == NULL) {
        errno = EINVAL;
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
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": malformed metadata in an JGF node for ";
        m_err_msg += std::string (f.vertex_id) + "\n";
        goto done;
    }
    if ( (p = json_object_get (metadata, "paths")) == NULL) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": key (paths) does not exist in an JGF node for ";
        m_err_msg += std::string (f.vertex_id) + ".\n";
        goto done;
    }
    *properties = json_object_get (metadata, "properties");
    *paths = p;
    if (*properties && !json_is_object (*properties)) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": key (properties) must be an object or null for ";
        m_err_msg += std::string (f.vertex_id) + ".\n";
        goto done;
    }
    rc = 0;
done:
    return rc;
}

int resource_reader_jgf_t::unpack_vtx (json_t *element, fetch_helper_t &f)
{
    json_t *paths = NULL;
    json_t *properties = NULL;
    if (fill_fetcher (element, f, &paths, &properties) < 0)
        return -1;
    if (remap_aware_unpack_vtx (f, paths, properties) < 0)
        return -1;
    return 0;
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
    g[v].rank = fetcher.get_proper_rank ();
    g[v].status = fetcher.status;
    g[v].id = fetcher.get_proper_id ();
    g[v].name = fetcher.get_proper_name ();
    g[v].properties = fetcher.properties;
    g[v].paths = fetcher.paths;
    g[v].schedule.plans = plans;
    g[v].idata.x_checker = x_checker;
    for (auto kv : g[v].paths)
        g[v].idata.member_of[kv.first] = "*";

done:
    return v;
}

vtx_t resource_reader_jgf_t::vtx_in_graph (const resource_graph_t &g,
                                           const resource_graph_metadata_t &m,
                                           const std::map<std::string,
                                                          std::string> &paths,
                                           int rank)
{
    for (auto const &paths_it : paths) {
        auto iter = m.by_path.find (paths_it.second);
        if (iter != m.by_path.end ()) {
            for (auto &v : iter->second) {
                if (g[v].rank == rank) {
                    return v;
                }
            }
        }
    }

    return boost::graph_traits<resource_graph_t>::null_vertex ();
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
            if (!ptr.second) {
                errno = EINVAL;
                m_err_msg += __FUNCTION__;
                m_err_msg += ": failed to add root metadata for ";
                m_err_msg += kv.first + " subsystem. ";
                m_err_msg += "Possible duplicate root.\n";
                goto done;
            }
        }
        m.by_path[kv.second].push_back (v);
    }
    m.by_type[g[v].type].push_back (v);
    m.by_name[g[v].name].push_back (v);
    m.by_rank[g[v].rank].push_back (v);
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::remove_graph_metadata (vtx_t v,
                                                  resource_graph_t &g,
                                                  resource_graph_metadata_t &m)
{
    int rc = -1;
    for (auto kv : g[v].paths) {
        m.by_path.erase (kv.second);
    }
    
    m.by_outedges.erase (v);

    for (auto it = m.by_type[g[v].type].begin (); it != m.by_type[g[v].type].end (); ++it) {
        if (*it == v) {
            m.by_type[g[v].type].erase (it);
            break;
        }
    }

    for (auto it = m.by_name[g[v].name].begin (); it != m.by_name[g[v].name].end (); ++it) {
        if (*it == v) {
            m.by_name[g[v].name].erase (it);
            break;
        }
    }

    for (auto it = m.by_rank[g[v].rank].begin (); it != m.by_rank[g[v].rank].end (); ++it) {
        if (*it == v) {
            m.by_rank[g[v].rank].erase (it);
            break;
        }
    }

    rc = 0;
    return rc;
}

int resource_reader_jgf_t::remove_metadata_outedges (vtx_t source_vertex,
                                                     vtx_t dest_vertex,
                                                     resource_graph_t &g,
                                                     resource_graph_metadata_t &m)
{
    int rc = -1;
    std::vector<edg_t> remove_edges;
    auto iter = m.by_outedges.find (source_vertex);
    if (iter == m.by_outedges.end ())
        return rc;
    auto &outedges = iter->second;
    for (auto kv = outedges.begin (); kv != outedges.end (); ++kv) {
        if (boost::target (kv->second, g) == dest_vertex) {
            kv = outedges.erase (kv); 
            // TODO: Consider adding break here 
        }
    }

    rc = 0;
    return rc;
}

int resource_reader_jgf_t::update_vmap (std::map<std::string,
                                                 vmap_val_t> &vmap,
                                        vtx_t v,
                                        const std::map<std::string,
                                                       bool> &root_checks,
                                        const fetch_helper_t &fetcher)
{
    int rc = -1;
    std::pair<std::map<std::string, vmap_val_t>::iterator, bool> ptr;
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

int resource_reader_jgf_t::add_vtx (resource_graph_t &g,
                                    resource_graph_metadata_t &m,
                                    std::map<std::string, vmap_val_t> &vmap,
                                    const fetch_helper_t &fetcher)
{
    int rc = -1;
    std::map<std::string, bool> root_checks;
    std::pair<std::map<std::string, vmap_val_t>::iterator, bool> ptr;
    vtx_t nullvtx = boost::graph_traits<resource_graph_t>::null_vertex ();
    vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();

    if (vmap.find (fetcher.vertex_id) != vmap.end ()) {
        errno = EINVAL;
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
    if ( (rc = update_vmap (vmap, v, root_checks, fetcher)) != 0)
        goto done;
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::exist (resource_graph_t &g,
                                  resource_graph_metadata_t &m,
                                  const std::string &path, int rank,
                                  const std::string &vid, vtx_t &v)
{
    try {
        auto &vect = m.by_path.at (path);
        for (auto &u : vect) {
            if (g[u].rank == rank) {
                v = u;
                return 0;
            }
        }
    } catch (std::out_of_range &e) {
        goto error;
    }

error:
    errno = ENOENT;
    m_err_msg += __FUNCTION__;
    m_err_msg += ": inconsistent input vertex: nonexistent path (";
    m_err_msg += path + ") " + vid + ".\n";
    return -1;
}

int resource_reader_jgf_t::find_vtx (resource_graph_t &g,
                                     resource_graph_metadata_t &m,
                                     std::map<std::string, vmap_val_t> &vmap,
                                     const fetch_helper_t &fetcher,
                                     vtx_t &v)
{
    int rc = -1;
    vtx_t u;
    vtx_t nullvtx = boost::graph_traits<resource_graph_t>::null_vertex ();
    v = nullvtx;

    if (vmap.find (fetcher.vertex_id) != vmap.end ()) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": found duplicate JGF node id for ";
        m_err_msg += std::string (fetcher.vertex_id) + ".\n";
        goto done;
    }

    for (auto &kv : fetcher.paths) {
        if (exist (g, m, kv.second, fetcher.rank, fetcher.vertex_id, u) < 0)
            goto done;
        if (v == nullvtx) {
            v = u;
        } else if (v != u) {
            errno = EINVAL;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": inconsistent input vertex for " + kv.second;
            m_err_msg += " (id=" + std::string (fetcher.vertex_id) + ").\n";
            m_err_msg += std::to_string (u) + " != " + std::to_string (v) + ".\n";
            goto done;
        }
    }

    if (g[v] != fetcher) {
        errno = EINVAL;
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
        m_err_msg += g[v].name + ".\n";
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
    std::map<std::string, bool> root_checks;
    vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();
    std::pair<std::map<std::string, vmap_val_t>::iterator, bool> ptr;

    if ( (rc = find_vtx (g, m, vmap, fetcher, v)) != 0)
        goto done;
    if ( (rc = check_root (v, g, root_checks)) != 0)
        goto done;
    if ( (rc = update_vmap (vmap, v, root_checks, fetcher)) != 0)
        goto done;
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
                                            json_t *nodes,
                                            std::unordered_set<std::string>
                                            &added_vtcs)
{
    int rc = -1;
    unsigned int i = 0;
    fetch_helper_t fetcher;
    vtx_t null_vtx = boost::graph_traits<resource_graph_t>::null_vertex ();
    std::map<std::string, bool> root_checks;
    std::pair<std::map<std::string, vmap_val_t>::iterator, bool> ptr;

    for (i = 0; i < json_array_size (nodes); i++) {
        fetcher.scrub ();
        if (unpack_vtx (json_array_get (nodes, i), fetcher) != 0)
            goto done;

        // If the vertex isn't in the graph, add it
        vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();
        if ( (v = vtx_in_graph (g, m,
                                fetcher.paths, fetcher.rank)) == null_vtx) {
            if (add_vtx (g, m, vmap, fetcher) != 0)
                goto done;
            auto res = added_vtcs.insert (std::string (fetcher.vertex_id));
            if (!res.second) {
                errno = EEXIST;
                m_err_msg += __FUNCTION__;
                m_err_msg += ": can't insert into added_vtcs for ";
                m_err_msg += std::string (fetcher.vertex_id) + ".\n";
                goto done;
            }
        } else {
            if ( (rc = update_vmap (vmap, v, root_checks, fetcher)) != 0)
                goto done;
        }
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

    for (i = 0; i < json_array_size (nodes); i++) {
        fetcher.scrub ();
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
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": encountered a malformed edge.\n";
        goto done;
    }
    source = src;
    target = tgt;
    if (vmap.find (source) == vmap.end ()
        || vmap.find (target) == vmap.end ()) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": source and/or target vertex not found";
        m_err_msg += source + std::string (" -> ") + target + ".\n";
        goto done;
    }
    if ( (metadata = json_object_get (element, "metadata")) == NULL) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": metadata key not found in an edge for ";
        m_err_msg += source + std::string (" -> ") + target + ".\n";
        goto done;
    }
    if ( (*name = json_object_get (metadata, "name")) == NULL) {
        errno = EINVAL;
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
                                         json_t *edges,
                                         const std::unordered_set
                                         <std::string> &added_vtcs)
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
        // We only add the edge when it connects at least one newly added vertex
        if ( (added_vtcs.count (source) == 1)
              || (added_vtcs.count (target) == 1)) {
            tie (e, inserted) = add_edge (vmap[source].v, vmap[target].v, g);
            if (inserted == false) {
                errno = EINVAL;
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
            // add this edge to by_outedges metadata
            auto iter = m.by_outedges.find (vmap[source].v);
            if (iter == m.by_outedges.end ()) {
                auto ret = m.by_outedges.insert (
                               std::make_pair (
                                   vmap[source].v,
                                   std::map<std::pair<uint64_t, int64_t>, edg_t,
                                            std::greater<
                                                     std::pair<uint64_t,
                                                               int64_t>>> ()));
                if (!ret.second) {
                    errno = ENOMEM;
                    m_err_msg += "error creating out-edge metadata map: "
                                      + g[vmap[source].v].name + " -> "
                                      + g[vmap[target].v].name + "; ";
                    goto done;
                }
                iter = m.by_outedges.find (vmap[source].v);
            }
            std::pair<uint64_t, int64_t> key = std::make_pair (
                                                   g[e].idata.get_weight (),
                                                   g[vmap[target].v].uniq_id);
            auto ret = iter->second.insert (std::make_pair (key, e));
            if (!ret.second) {
                errno = EEXIST;
                m_err_msg += "error inserting an edge to outedge metadata map: "
                             + g[vmap[source].v].name + " -> "
                             + g[vmap[target].v].name + "; ";
                goto done;
            }
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
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": JGF edge not found in resource graph.\n";
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
    std::string source{};
    std::string target{};

    for (i = 0; i < json_array_size (edges); i++) {
        element = json_array_get (edges, i);
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

int resource_reader_jgf_t::get_subgraph_vertices (resource_graph_t &g,
                                                  vtx_t vtx,
                                                  std::vector<vtx_t> &vtx_list)
{
    vtx_t next_vtx;
    boost::graph_traits<resource_graph_t>::out_edge_iterator ei, ei_end;
    boost::tie (ei, ei_end) = boost::out_edges (vtx, g);

    for (; ei != ei_end; ++ei) {
        next_vtx =  boost::target (*ei, g);
        
        for (auto const &paths_it : g[next_vtx].paths) {
            // check that we don't recurse on parent edges 
            if (paths_it.second.find (g[vtx].name) != std::string::npos &&
                paths_it.second.find (g[vtx].name) < paths_it.second.find (g[next_vtx].name)) {
                vtx_list.push_back (next_vtx);
                get_subgraph_vertices (g, next_vtx, vtx_list);
                break;
            }
        }      
  }

  return 0;
}

int resource_reader_jgf_t::get_parent_vtx (resource_graph_t &g,
                                           vtx_t vtx,
                                           vtx_t &parent_vtx)
                                            
{
    vtx_t next_vtx;
    boost::graph_traits<resource_graph_t>::out_edge_iterator ei, ei_end;
    boost::tie (ei, ei_end) = boost::out_edges (vtx, g);
    int rc = -1;

    for (; ei != ei_end; ++ei) {
        next_vtx =  boost::target (*ei, g);
        for (auto const &paths_it : g[vtx].paths) {
            // check that the parent's name exists in the child's path before the child's name
            if (paths_it.second.find (g[next_vtx].name) != std::string::npos &&
                paths_it.second.find (g[vtx].name) > paths_it.second.find (g[next_vtx].name)) {
                parent_vtx = next_vtx;
                rc = 0;
                break;
            }
        }    
  }

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
    std::unordered_set<std::string> added_vtcs;

    if (rank != -1) {
        errno = ENOTSUP;
        m_err_msg += __FUNCTION__;
        m_err_msg += "rank != -1 unsupported for JGF unpack.\n";
        goto done;
    }
    if ( (rc = fetch_jgf (str, &jgf, &nodes, &edges)) != 0)
        goto done;
    if ( (rc = unpack_vertices (g, m, vmap, nodes, added_vtcs)) != 0)
        goto done;
    if ( (rc = unpack_edges (g, m, vmap, edges, added_vtcs)) != 0)
        goto done;

done:
    json_decref (jgf);
    return rc;
}

int resource_reader_jgf_t::unpack_at (resource_graph_t &g,
                                      resource_graph_metadata_t &m, vtx_t &vtx,
                                      const std::string &str, int rank)
{
    /* This functionality is currently experimental, as resource graph
     * growth causes a resize of the boost vecS vertex container type.
     * Resizing the vecS results in lost job allocations and reservations
     * as there is no copy constructor for planner.
     * vtx_t vtx is not implemented and may be used in the future
     * for optimization.
     */

    return unpack (g, m, str, rank);
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

int resource_reader_jgf_t::remove_subgraph (resource_graph_t &g,
                                            resource_graph_metadata_t &m,
                                            const std::string &path)
{
    vtx_t subgraph_root_vtx = boost::graph_traits<resource_graph_t>::null_vertex ();
    vtx_t parent_vtx = boost::graph_traits<resource_graph_t>::null_vertex ();
    std::vector<vtx_t> vtx_list;

    auto iter = m.by_path.find (path);
    if (iter == m.by_path.end ()) {
        return -1;
    }

    for (auto &v : iter->second) {
        subgraph_root_vtx = v;
    }

    vtx_list.push_back (subgraph_root_vtx);

    get_subgraph_vertices (g, subgraph_root_vtx, vtx_list);
    
    if ( get_parent_vtx (g, subgraph_root_vtx, parent_vtx) )
        return -1;
    
    if ( remove_metadata_outedges (parent_vtx, subgraph_root_vtx, g, m) )
        return -1;

    for (auto & vtx : vtx_list)
    {   
        // clear vertex edges but don't delete vertex
        boost::clear_vertex (vtx, g);
        remove_graph_metadata (vtx, g, m);
    }

    return 0;

}

bool resource_reader_jgf_t::is_allowlist_supported ()
{
    return false;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
