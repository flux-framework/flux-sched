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
#include <flux/idset.h>
}

#include <map>
#include <unordered_set>
#include <unistd.h>
#include <regex>
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

    int64_t id = -2;
    int64_t rank = -1;
    int64_t size = -1;
    int64_t uniq_id = -1;
    int exclusive = -1;
    resource_pool_t::status_t status = resource_pool_t::status_t::UP;
    const char *type = NULL;
    std::string name;
    const char *unit = NULL;
    const char *basename = NULL;
    const char *vertex_id = NULL;
    std::map<std::string, std::string> properties;
    std::map<subsystem_t, std::string> paths;
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
    return (is_name_remapped ()) ? get_remapped_name ().c_str () : name.c_str ();
}

int64_t fetch_helper_t::get_proper_id () const
{
    return (is_id_remapped ()) ? get_remapped_id () : id;
}

int64_t fetch_helper_t::get_proper_rank () const
{
    return (is_rank_remapped ()) ? get_remapped_rank () : rank;
}

void fetch_helper_t::scrub ()
{
    id = -2;
    rank = -1;
    size = -1;
    uniq_id = -1;
    exclusive = -1;
    status = resource_pool_t::status_t::UP;
    type = NULL;
    name.clear ();
    unit = NULL;
    basename = NULL;
    vertex_id = NULL;
    properties.clear ();
    paths.clear ();
    clear ();
}

struct vmap_val_t {
    vtx_t v;
    std::map<subsystem_t, bool> is_roots;
    unsigned int needs;
    unsigned int exclusive;
};

bool operator== (const resource_pool_t &r, const fetch_helper_t &f)
{
    return (r.type.get () == f.type && r.basename == f.basename
            && r.size == static_cast<unsigned int> (f.size) && r.rank == static_cast<int> (f.rank)
            && r.id == f.id && r.name == f.name && r.properties == f.properties
            && r.paths == f.paths);
}

bool operator!= (const resource_pool_t &r, const fetch_helper_t &f)
{
    return !(r == f);
}

std::string diff (const resource_pool_t &r, const fetch_helper_t &f)
{
    std::stringstream sstream;
    if (r.type.get () != f.type)
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
        for (const auto &kv : r.properties)
            sstream << kv.first << "=" << kv.second << " ";
        sstream << ", ";
        for (const auto &kv : f.properties)
            sstream << kv.first << "=" << kv.second << " ";
        sstream << ")";
    }
    if (r.paths != f.paths) {
        sstream << " paths=(";
        for (const auto &kv : r.paths)
            sstream << kv.first << "=" << kv.second << " ";
        sstream << ", ";
        for (const auto &kv : f.paths)
            sstream << kv.first << "=" << kv.second << " ";
        sstream << ")";
    }
    return sstream.str ();
}

////////////////////////////////////////////////////////////////////////////////
// Private JGF Resource Reader
////////////////////////////////////////////////////////////////////////////////

int resource_reader_jgf_t::fetch_jgf (const std::string &str,
                                      json_t **jgf_p,
                                      json_t **nodes_p,
                                      json_t **edges_p,
                                      jgf_updater_data &update_data)
{
    int rc = -1;
    int64_t rank;
    json_t *graph = NULL;
    json_t *free_ranks = NULL;
    struct idset *r_ids = nullptr;
    const char *ranks = nullptr;
    std::string ranks_stripped;
    json_error_t json_err;

    if ((*jgf_p = json_loads (str.c_str (), 0, &json_err)) == NULL) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": json_loads returned an error: ";
        m_err_msg += std::string (json_err.text) + std::string (": ");
        m_err_msg += std::string (json_err.source) + std::string ("@")
                     + std::to_string (json_err.line) + std::string (":")
                     + std::to_string (json_err.column) + ".\n";
        goto done;
    }
    if ((graph = json_object_get (*jgf_p, "graph")) == NULL) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": JGF does not contain a required key (graph).\n";
        goto done;
    }
    if ((free_ranks = json_object_get (*jgf_p, "free_ranks")) != NULL) {
        update_data.isect_ranks = true;
        if (!(ranks = json_dumps (free_ranks, JSON_ENCODE_ANY | JSON_COMPACT))) {
            errno = ENOMEM;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": json_dumps failed.\n";
            goto done;
        }
        // Need to strip double quotes inserted by json_dumps above
        ranks_stripped = std::string (ranks);
        ranks_stripped.erase (std::remove (ranks_stripped.begin (), ranks_stripped.end (), '"'),
                              ranks_stripped.end ());
        if ((r_ids = idset_decode (ranks_stripped.c_str ())) == NULL) {
            errno = EINVAL;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": failed to decode ranks.\n";
            goto done;
        }
        rank = idset_first (r_ids);
        while (rank != IDSET_INVALID_ID) {
            update_data.ranks.insert (rank);
            rank = idset_next (r_ids, rank);
        }
    }
    if ((*nodes_p = json_object_get (graph, "nodes")) == NULL) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": JGF does not contain a required key (nodes).\n";
        goto done;
    }
    if ((*edges_p = json_object_get (graph, "edges")) == NULL) {
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

    if (namespace_remapper.query_exec_target (static_cast<uint64_t> (f.rank), remap_rank) < 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": error remapping rank id=";
        m_err_msg += std::to_string (f.rank) + ".\n";
        goto error;
    }
    if (remap_rank > static_cast<uint64_t> (std::numeric_limits<int64_t>::max ())) {
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
        if (remap_id > static_cast<uint64_t> (std::numeric_limits<int>::max ())) {
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
            if (sl == std::string::npos || path.substr (sl + 1, 4) != "core") {
                errno = EINVAL;
                m_err_msg += __FUNCTION__;
                m_err_msg += ": malformed path for core id=";
                m_err_msg += std::to_string (f.id) + ".\n";
                goto error;
            }
            f.paths[subsystem_t (key)] =
                path.substr (0, sl + 1) + f.basename + std::to_string (remap_id);
        }
    } else {
        json_object_foreach (paths, key, value) {
            f.paths[subsystem_t (key)] = json_string_value (value);
        }
    }

    json_object_foreach (properties, key, value) {
        f.properties[std::string (key)] = std::string (json_string_value (value));
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
            f.paths[subsystem_t (key)] = std::string (json_string_value (value));
        }
        json_object_foreach (properties, key, value) {
            f.properties[std::string (key)] = std::string (json_string_value (value));
        }
    }
    return 0;
}

int resource_reader_jgf_t::apply_defaults (fetch_helper_t &f, const char *name)
{
    if (f.uniq_id == -1) {
        try {
            f.uniq_id = static_cast<int64_t> (std::stoll (std::string{f.vertex_id}));
        } catch (std::invalid_argument const &ex) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": value for key (uniq_id) could not be inferred from outer ";
            m_err_msg += "'id' field " + std::string (f.vertex_id) + ".\n";
            return -1;
        }
    }
    if (f.id == -2) {
        f.id = f.uniq_id;
        // for nodes, see if there is an integer suffix on the hostname and use it if so
        if (f.type == std::string{"node"} && name != NULL) {
            std::string sname{name};
            std::regex nodesuffix ("(\\d+$)");
            std::smatch r;
            if (std::regex_search (sname, r, nodesuffix)) {
                try {
                    f.id = std::stoll (r.str (0));
                } catch (std::invalid_argument const &ex) {
                    m_err_msg += __FUNCTION__;
                    m_err_msg += ": could not extract ID from hostname ";
                    m_err_msg += sname;
                    return -1;
                }
            }
        }
    }
    if (f.exclusive == -1)
        f.exclusive = 0;
    if (f.size == -1)
        f.size = 1;
    if (f.basename == NULL)
        f.basename = f.type;
    if (name == NULL) {
        f.name = f.basename;
        f.name.append (std::to_string (f.id));
    } else {
        f.name = name;
    }
    if (f.unit == NULL)
        f.unit = "";
    return 0;
}

int resource_reader_jgf_t::fill_fetcher (json_t *element,
                                         fetch_helper_t &f,
                                         json_t **paths,
                                         json_t **properties)
{
    json_t *metadata = NULL;
    const char *name = NULL;

    if ((json_unpack (element, "{ s:s }", "id", &f.vertex_id) < 0)) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": JGF vertex id key is not found in a node.\n";
        return -1;
    }
    if ((metadata = json_object_get (element, "metadata")) == NULL) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": key (metadata) is not found in an JGF node for ";
        m_err_msg += std::string (f.vertex_id) + ".\n";
        return -1;
    }
    if ((json_unpack (metadata,
                      "{ s:s s?s s?s s?I s?I s?I s?i s?b s?s s?I s:o s?o }",
                      "type",
                      &f.type,
                      "basename",
                      &f.basename,
                      "name",
                      &name,
                      "id",
                      &f.id,
                      "uniq_id",
                      &f.uniq_id,
                      "rank",
                      &f.rank,
                      "status",
                      &f.status,
                      "exclusive",
                      &f.exclusive,
                      "unit",
                      &f.unit,
                      "size",
                      &f.size,
                      "paths",
                      paths,
                      "properties",
                      properties))
        < 0) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": malformed metadata in an JGF node for ";
        m_err_msg += std::string (f.vertex_id) + "\n";
        return -1;
    }
    if (*properties && !json_is_object (*properties)) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": key (properties) must be an object or null for ";
        m_err_msg += std::string (f.vertex_id) + ".\n";
        return -1;
    }
    return apply_defaults (f, name);
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

vtx_t resource_reader_jgf_t::create_vtx (resource_graph_t &g, const fetch_helper_t &fetcher)
{
    planner_t *plans = NULL;
    planner_t *x_checker = NULL;
    vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();

    if (!(plans = planner_new (0, INT64_MAX, fetcher.size, fetcher.type))) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": planner_new returned NULL.\n";
        goto done;
    }
    if (!(x_checker = planner_new (0, INT64_MAX, X_CHECKER_NJOBS, X_CHECKER_JOBS_STR))) {
        m_err_msg += __FUNCTION__;
        m_err_msg += "planner_new for x_checker returned NULL.\n";
        goto done;
    }

    v = boost::add_vertex (g);
    g[v].type = resource_type_t{fetcher.type};
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
    for (const auto &kv : g[v].paths)
        g[v].idata.member_of[subsystem_t{kv.first}] = "*";

done:
    return v;
}

vtx_t resource_reader_jgf_t::vtx_in_graph (const resource_graph_t &g,
                                           const resource_graph_metadata_t &m,
                                           const std::map<subsystem_t, std::string> &paths,
                                           int rank)
{
    for (const auto &paths_it : paths) {
        auto iter = m.by_path.find (paths_it.second);
        if (iter != m.by_path.end ()) {
            for (const auto &v : iter->second) {
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

int resource_reader_jgf_t::check_root (vtx_t v,
                                       resource_graph_t &g,
                                       std::map<subsystem_t, bool> &is_roots)
{
    int rc = -1;
    for (const auto &kv : g[v].paths) {
        if (is_root (kv.second)) {
            auto ptr = is_roots.emplace (kv.first, true);
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

    for (const auto &kv : g[v].paths) {
        if (is_root (kv.second)) {
            auto ptr = m.roots.emplace (kv.first, v);
            if (!ptr.second) {
                errno = EINVAL;
                m_err_msg += __FUNCTION__;
                m_err_msg += ": failed to add root metadata for ";
                m_err_msg += kv.first.get () + " subsystem. ";
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

int resource_reader_jgf_t::update_vmap (std::map<std::string, vmap_val_t> &vmap,
                                        vtx_t v,
                                        const std::map<subsystem_t, bool> &root_checks,
                                        const fetch_helper_t &fetcher)
{
    int rc = -1;
    std::pair<std::map<std::string, vmap_val_t>::iterator, bool> ptr;
    ptr = vmap.emplace (std::string (fetcher.vertex_id),
                        vmap_val_t{v,
                                   root_checks,
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
    std::map<subsystem_t, bool> root_checks;
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
    if ((v = create_vtx (g, fetcher)) == nullvtx)
        goto done;
    if ((rc = check_root (v, g, root_checks)) == -1)
        goto done;
    if ((rc = add_graph_metadata (v, g, m)) == -1)
        goto done;
    if ((rc = update_vmap (vmap, v, root_checks, fetcher)) != 0)
        goto done;
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::exist (resource_graph_t &g,
                                  resource_graph_metadata_t &m,
                                  const std::string &path,
                                  int rank,
                                  const std::string &vid,
                                  vtx_t &v)
{
    try {
        auto &vect = m.by_path.at (path);
        for (const auto &u : vect) {
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

    for (const auto &kv : fetcher.paths) {
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

int resource_reader_jgf_t::update_vtx_plan (vtx_t v,
                                            resource_graph_t &g,
                                            const fetch_helper_t &fetcher,
                                            jgf_updater_data &update_data)
{
    int rc = -1;
    int64_t span = -1;
    int64_t avail = -1;
    planner_t *plans = NULL;

    if ((plans = g[v].schedule.plans) == NULL) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": plan for " + g[v].name + " is null.\n";
        goto done;
    }
    if ((avail = planner_avail_resources_during (plans, update_data.at, update_data.duration))
        == -1) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": planner_avail_resource_during return -1 for ";
        m_err_msg += g[v].name + ".\n";
        goto done;
    }

    if (fetcher.exclusive) {
        // Update the vertex plan here (not in traverser code) so vertices
        // that the traverser won't walk still get their plans updated.
        if ((span = planner_add_span (plans,
                                      update_data.at,
                                      update_data.duration,
                                      static_cast<const uint64_t> (g[v].size)))
            == -1) {
            m_err_msg += __FUNCTION__;
            m_err_msg += ": can't add span into " + g[v].name + ".\n";
            goto done;
        }
        if (update_data.reserved)
            g[v].schedule.reservations[update_data.jobid] = span;
        else
            g[v].schedule.allocations[update_data.jobid] = span;
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

int resource_reader_jgf_t::cancel_vtx (vtx_t vtx,
                                       resource_graph_t &g,
                                       resource_graph_metadata_t &m,
                                       const fetch_helper_t &fetcher,
                                       jgf_updater_data &update_data)
{
    int64_t span = -1;
    int64_t xspan = -1;
    int64_t sched_span = -1;
    int64_t prev_occu = -1;
    planner_multi_t *subtree_plan = NULL;
    planner_t *x_checker = NULL;
    planner_t *plans = NULL;
    auto &job2span = g[vtx].idata.job2span;
    auto &x_spans = g[vtx].idata.x_spans;
    auto &tags = g[vtx].idata.tags;
    std::map<int64_t, int64_t>::iterator span_it;
    std::map<int64_t, int64_t>::iterator xspan_it;
    uint32_t path_len = 0;
    rank_data *rdata = nullptr;

    static const subsystem_t containment_sub{"containment"};
    // remove from aggregate filter if present
    auto agg_span = job2span.find (update_data.jobid);
    if (agg_span != job2span.end ()) {
        if ((subtree_plan = g[vtx].idata.subplans[containment_sub]) == NULL)
            goto error;
        if (planner_multi_rem_span (subtree_plan, agg_span->second) != 0)
            goto error;
        // Delete from job2span tracker
        job2span.erase (update_data.jobid);
    }

    // remove from exclusive filter;
    xspan_it = x_spans.find (update_data.jobid);
    if (xspan_it == x_spans.end ()) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": can't find x_checker span for job ";
        m_err_msg += std::to_string (update_data.jobid) + " in " + g[vtx].name + ".\n";
        goto error;
    }
    xspan = xspan_it->second;
    x_checker = g[vtx].idata.x_checker;
    g[vtx].idata.tags.erase (update_data.jobid);
    g[vtx].idata.x_spans.erase (update_data.jobid);
    if (planner_rem_span (x_checker, xspan) == -1) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": can't remove x_checker span for job ";
        m_err_msg += std::to_string (update_data.jobid) + " in " + g[vtx].name + ".\n";
        goto error;
    }
    // rem plan
    span_it = g[vtx].schedule.allocations.find (update_data.jobid);
    sched_span = span_it->second;
    if (span_it != g[vtx].schedule.allocations.end ()) {
        g[vtx].schedule.allocations.erase (update_data.jobid);
    } else {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": can't find allocation span for job ";
        m_err_msg += std::to_string (update_data.jobid) + " in " + g[vtx].name + ".\n";
        goto error;
    }
    plans = g[vtx].schedule.plans;
    if ((prev_occu = planner_span_resource_count (plans, sched_span)) < 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": planner_span_resource_count failed for job ";
        m_err_msg += std::to_string (update_data.jobid) + " in " + g[vtx].name + ".\n";
        goto error;
    }
    if (planner_rem_span (plans, sched_span) == -1) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": can't remove allocation span for job ";
        m_err_msg += std::to_string (update_data.jobid) + " in " + g[vtx].name + ".\n";
        goto error;
    }
    // Don't need to check if rank is invalid; check done in find_vtx ().
    // Add the newly freed counts, Can't assume it freed everything.
    rdata = &(update_data.rank_to_data[g[vtx].rank]);
    rdata->type_to_count[g[vtx].type] += prev_occu;
    update_data.ranks.insert (g[vtx].rank);
    path_len = g[vtx].paths.at (containment_sub).length ();
    if (rdata->length > path_len) {
        rdata->length = path_len;
        rdata->root = vtx;
    }

    return 0;
error:
    errno = EINVAL;
    return -1;
}

int resource_reader_jgf_t::update_vtx (resource_graph_t &g,
                                       resource_graph_metadata_t &m,
                                       std::map<std::string, vmap_val_t> &vmap,
                                       const fetch_helper_t &fetcher,
                                       jgf_updater_data &update_data)
{
    int rc = -1;
    std::map<subsystem_t, bool> root_checks;
    vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();
    std::pair<std::map<std::string, vmap_val_t>::iterator, bool> ptr;

    if ((rc = find_vtx (g, m, vmap, fetcher, v)) != 0)
        goto done;
    if ((rc = check_root (v, g, root_checks)) != 0)
        goto done;
    // Check if skipping due to previous partial free
    if (update_data.isect_ranks && !update_data.ranks.empty ()) {
        if (update_data.ranks.find (fetcher.rank) != update_data.ranks.end ()) {
            rc = 0;
            goto done;
        }
    }
    if ((rc = update_vmap (vmap, v, root_checks, fetcher)) != 0)
        goto done;
    if (update_data.update) {
        if ((rc = update_vtx_plan (v, g, fetcher, update_data)) != 0)
            goto done;
    } else {
        if ((rc = cancel_vtx (v, g, m, fetcher, update_data)) != 0)
            goto done;
    }

done:
    return rc;
}

int resource_reader_jgf_t::undo_vertices (resource_graph_t &g,
                                          std::map<std::string, vmap_val_t> &vmap,
                                          jgf_updater_data &update_data)
{
    int rc = 0;
    int rc2 = 0;
    int64_t span = -1;
    planner_t *plans = NULL;
    vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();

    for (const auto &kv : vmap) {
        if (kv.second.exclusive != 1)
            continue;
        try {
            v = kv.second.v;
            if (update_data.reserved) {
                span = g[v].schedule.reservations.at (update_data.jobid);
                g[v].schedule.reservations.erase (update_data.jobid);
            } else {
                span = g[v].schedule.allocations.at (update_data.jobid);
                g[v].schedule.allocations.erase (update_data.jobid);
            }

            plans = g[v].schedule.plans;
            if ((rc2 = planner_rem_span (plans, span)) == -1) {
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

    return (!rc) ? 0 : -1;
}

int resource_reader_jgf_t::unpack_vertices (resource_graph_t &g,
                                            resource_graph_metadata_t &m,
                                            std::map<std::string, vmap_val_t> &vmap,
                                            json_t *nodes,
                                            std::unordered_set<std::string> &added_vtcs)
{
    int rc = -1;
    unsigned int i = 0;
    fetch_helper_t fetcher;
    vtx_t null_vtx = boost::graph_traits<resource_graph_t>::null_vertex ();
    std::map<subsystem_t, bool> root_checks;
    std::pair<std::map<std::string, vmap_val_t>::iterator, bool> ptr;

    for (i = 0; i < json_array_size (nodes); i++) {
        fetcher.scrub ();
        if (unpack_vtx (json_array_get (nodes, i), fetcher) != 0)
            goto done;

        // If the vertex isn't in the graph, add it
        vtx_t v = boost::graph_traits<resource_graph_t>::null_vertex ();
        if ((v = vtx_in_graph (g, m, fetcher.paths, fetcher.rank)) == null_vtx) {
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
            if ((rc = update_vmap (vmap, v, root_checks, fetcher)) != 0)
                goto done;
        }
    }
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::update_vertices (resource_graph_t &g,
                                            resource_graph_metadata_t &m,
                                            std::map<std::string, vmap_val_t> &vmap,
                                            json_t *nodes,
                                            jgf_updater_data &update_data)
{
    int rc = -1;
    unsigned int i = 0;
    fetch_helper_t fetcher;

    for (i = 0; i < json_array_size (nodes); i++) {
        fetcher.scrub ();
        if ((rc = unpack_vtx (json_array_get (nodes, i), fetcher)) != 0)
            goto done;
        if ((rc = update_vtx (g, m, vmap, fetcher, update_data)) != 0)
            goto done;
    }
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::unpack_edge (json_t *element,
                                        std::map<std::string, vmap_val_t> &vmap,
                                        std::string &source,
                                        std::string &target,
                                        std::string &subsystem,
                                        jgf_updater_data &update_data)
{
    int rc = -1;
    json_t *metadata = NULL;
    const char *src = NULL;
    const char *tgt = NULL;
    const char *subsys = "containment";

    if ((json_unpack (element, "{ s:s s:s }", "source", &src, "target", &tgt)) < 0) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": encountered a malformed edge.\n";
        goto done;
    }
    source = src;
    target = tgt;
    if (vmap.find (source) == vmap.end () || vmap.find (target) == vmap.end ()) {
        if (update_data.isect_ranks) {
            update_data.skipped = true;
            rc = 0;
            goto done;
        } else {
            errno = EINVAL;
            m_err_msg += __FUNCTION__;
            m_err_msg += ": source and/or target vertex not found";
            m_err_msg += source + std::string (" -> ") + target + ".\n";
            goto done;
        }
    }
    if ((json_unpack (element, "{ s?{ s?s } }", "metadata", "subsystem", &subsys)) < 0) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": could not unpack edge metadata.\n";
        goto done;
    }
    subsystem = subsys;
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::unpack_edges (resource_graph_t &g,
                                         resource_graph_metadata_t &m,
                                         std::map<std::string, vmap_val_t> &vmap,
                                         json_t *edges,
                                         const std::unordered_set<std::string> &added_vtcs)
{
    edg_t e;
    int rc = -1;
    unsigned int i = 0;
    json_t *element = NULL;
    json_t *value = NULL;
    bool inserted = false;
    const char *key = NULL;
    std::string source{};
    std::string target{};
    std::string subsystem{};
    jgf_updater_data update_data;

    for (i = 0; i < json_array_size (edges); i++) {
        element = json_array_get (edges, i);
        if ((unpack_edge (element, vmap, source, target, subsystem, update_data)) != 0)
            goto done;
        // We only add the edge when it connects at least one newly added vertex
        if ((added_vtcs.count (source) == 1) || (added_vtcs.count (target) == 1)) {
            tie (e, inserted) = add_edge (vmap[source].v, vmap[target].v, g);
            if (inserted == false) {
                errno = EINVAL;
                m_err_msg += __FUNCTION__;
                m_err_msg += ": couldn't add an edge to the graph for ";
                m_err_msg += source + std::string (" -> ") + target + ".\n";
                goto done;
            }
            g[e].subsystem = subsystem_t{subsystem};
            g[e].idata.member_of[subsystem_t{subsystem}] = true;
            // add this edge to by_outedges metadata
            auto iter = m.by_outedges.find (vmap[source].v);
            if (iter == m.by_outedges.end ()) {
                auto ret = m.by_outedges.insert (
                    std::make_pair (vmap[source].v,
                                    std::map<std::pair<uint64_t, int64_t>,
                                             edg_t,
                                             std::greater<std::pair<uint64_t, int64_t>>> ()));
                if (!ret.second) {
                    errno = ENOMEM;
                    m_err_msg += "error creating out-edge metadata map: " + g[vmap[source].v].name
                                 + " -> " + g[vmap[target].v].name + "; ";
                    goto done;
                }
                iter = m.by_outedges.find (vmap[source].v);
            }
            std::pair<uint64_t, int64_t> key =
                std::make_pair (g[e].idata.get_weight (), g[vmap[target].v].uniq_id);
            auto ret = iter->second.insert (std::make_pair (key, e));
            if (!ret.second) {
                errno = EEXIST;
                m_err_msg += "error inserting an edge to outedge metadata map: "
                             + g[vmap[source].v].name + " -> " + g[vmap[target].v].name + "; ";
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
                                            std::map<std::string, vmap_val_t> &vmap,
                                            std::string &source,
                                            uint64_t token)
{
    if (vmap[source].is_roots.empty ())
        return 0;

    for (const auto &kv : vmap[source].is_roots)
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
                                            std::map<std::string, vmap_val_t> &vmap,
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
    g[e].idata.set_for_trav_update (vmap[target].needs, vmap[target].exclusive, token);
    rc = 0;

done:
    return rc;
}

int resource_reader_jgf_t::update_edges (resource_graph_t &g,
                                         resource_graph_metadata_t &m,
                                         std::map<std::string, vmap_val_t> &vmap,
                                         json_t *edges,
                                         uint64_t token,
                                         jgf_updater_data &update_data)
{
    edg_t e;
    int rc = -1;
    unsigned int i = 0;
    json_t *element = NULL;
    std::string source{};
    std::string target{};
    std::string subsystem{};

    for (i = 0; i < json_array_size (edges); i++) {
        element = json_array_get (edges, i);
        // We only check protocol errors in JGF edges in the following...
        update_data.skipped = false;
        if ((rc = unpack_edge (element, vmap, source, target, subsystem, update_data)) != 0)
            goto done;
        if (update_data.skipped) {
            update_data.skipped = false;
            continue;
        }
        if ((rc = update_src_edge (g, m, vmap, source, token)) != 0)
            goto done;
        if ((rc = update_tgt_edge (g, m, vmap, source, target, token)) != 0)
            goto done;
    }

done:
    return rc;
}

////////////////////////////////////////////////////////////////////////////////
// Public JGF Resource Reader Interface
////////////////////////////////////////////////////////////////////////////////

resource_reader_jgf_t::~resource_reader_jgf_t ()
{
}

int resource_reader_jgf_t::unpack (resource_graph_t &g,
                                   resource_graph_metadata_t &m,
                                   const std::string &str,
                                   int rank)

{
    int rc = -1;
    json_t *jgf = NULL;
    json_t *nodes = NULL;
    json_t *edges = NULL;
    std::map<std::string, vmap_val_t> vmap;
    std::unordered_set<std::string> added_vtcs;
    jgf_updater_data update_data;

    if (rank != -1) {
        errno = ENOTSUP;
        m_err_msg += __FUNCTION__;
        m_err_msg += "rank != -1 unsupported for JGF unpack.\n";
        goto done;
    }
    if ((rc = fetch_jgf (str, &jgf, &nodes, &edges, update_data)) != 0)
        goto done;
    if ((rc = unpack_vertices (g, m, vmap, nodes, added_vtcs)) != 0)
        goto done;
    if ((rc = unpack_edges (g, m, vmap, edges, added_vtcs)) != 0)
        goto done;

done:
    json_decref (jgf);
    return rc;
}

int resource_reader_jgf_t::unpack_at (resource_graph_t &g,
                                      resource_graph_metadata_t &m,
                                      vtx_t &vtx,
                                      const std::string &str,
                                      int rank)
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
                                   const std::string &str,
                                   int64_t jobid,
                                   int64_t at,
                                   uint64_t dur,
                                   bool rsv,
                                   uint64_t token)
{
    int rc = -1;
    json_t *jgf = NULL;
    json_t *nodes = NULL;
    json_t *edges = NULL;
    std::map<std::string, vmap_val_t> vmap;
    jgf_updater_data update_data;

    if (at < 0 || dur == 0) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg +=
            ": invalid time (" + std::to_string (at) + ", " + std::to_string (dur) + ").\n";
        goto done;
    }

    // Fill in updater data
    update_data.jobid = jobid;
    update_data.at = at;
    update_data.duration = dur;
    update_data.reserved = rsv;
    update_data.update = true;

    if ((rc = fetch_jgf (str, &jgf, &nodes, &edges, update_data)) != 0)
        goto done;
    if ((rc = update_vertices (g, m, vmap, nodes, update_data)) != 0) {
        undo_vertices (g, vmap, update_data);
        goto done;
    }
    if ((rc = update_edges (g, m, vmap, edges, token, update_data)) != 0)
        goto done;

done:
    json_decref (jgf);
    return rc;
}

int resource_reader_jgf_t::partial_cancel (resource_graph_t &g,
                                           resource_graph_metadata_t &m,
                                           modify_data_t &mod_data,
                                           const std::string &R,
                                           int64_t jobid)
{
    int rc = -1;
    json_t *jgf = NULL;
    json_t *nodes = NULL;
    json_t *edges = NULL;
    std::map<std::string, vmap_val_t> vmap;
    jgf_updater_data p_cancel_data;

    if (jobid <= 0) {
        errno = EINVAL;
        m_err_msg += __FUNCTION__;
        m_err_msg += ": invalid jobid\n";
        goto done;
    }

    // Fill in updater data
    p_cancel_data.jobid = jobid;
    p_cancel_data.update = false;
    if ((rc = fetch_jgf (R, &jgf, &nodes, &edges, p_cancel_data)) != 0)
        goto done;
    if ((rc = update_vertices (g, m, vmap, nodes, p_cancel_data)) != 0)
        goto done;

    for (const auto &[rank, data] : p_cancel_data.rank_to_data) {
        mod_data.rank_to_counts[rank] = data.type_to_count;
        mod_data.ranks.insert (rank);
        mod_data.rank_to_root[rank] = data.root;
    }

done:
    json_decref (jgf);
    return rc;
}

bool resource_reader_jgf_t::is_allowlist_supported ()
{
    return false;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
