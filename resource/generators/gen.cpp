/*****************************************************************************\
 *  Copyright (c) 2014 Lawrence Livermore National Security, LLC.  Produced at
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
#include <deque>
#include <vector>
#include <cstdint>
#include <boost/algorithm/string.hpp>
#include "resource/generators/gen.hpp"
#include "resource/planner/planner.h"

extern "C" {
#include <hwloc.h>

#if HAVE_CONFIG_H
#include "config.h"
#endif
}

using namespace std;
using namespace Flux::resource_model;

/*! Note that this class must be copy-constructible
 * required by the concept of the depth first search
 * visitor. It must be lightweight.
 */
class dfs_emitter_t : public boost::default_dfs_visitor {
public:
    dfs_emitter_t ();
    dfs_emitter_t (resource_graph_db_t *db_p, resource_gen_spec_t *g);
    dfs_emitter_t (const dfs_emitter_t &o);
    dfs_emitter_t &operator= (const dfs_emitter_t &o);
    ~dfs_emitter_t ();

    void tree_edge (gge_t e, const gg_t &recipe);
    void finish_vertex (ggv_t u, const gg_t &recipe);
    const string &err_message () const;

private:
    vtx_t emit_vertex (ggv_t u, gge_t e, const gg_t &recipe,
                       vtx_t src_v, int i, int sz, int j);
    edg_t raw_edge (vtx_t src_v, vtx_t tgt_v);
    void emit_edges (gge_t e, const gg_t &recipe,
                     vtx_t src_v, vtx_t tgt_v);
    int path_prefix (const std::string &pth,
                     int upl, std::string &pref);
    int gen_id (gge_t e, const gg_t &recipe,
                int i, int sz, int j);

    map<ggv_t, vector<vtx_t>> m_gen_src_vtx;
    deque<int> m_hier_scales;
    resource_graph_db_t *m_db_p = NULL;
    resource_gen_spec_t *m_gspec_p = NULL;
    string m_err_msg = "";
};


/********************************************************************************
 *                                                                              *
 *                   Private DFS Visitor Emitter API                            *
 *                                                                              *
 ********************************************************************************/

int dfs_emitter_t::path_prefix (const string &path, int uplevel, string &prefix)
{
    size_t pos = 0;
    unsigned int occurrence = 0;
    auto num_slashes = count (path.begin (), path.end (), '/');
    if (uplevel >= num_slashes)
      return -1;
    while (occurrence != (num_slashes - uplevel + 1)) {
        pos = path.find ("/", pos);
        if (pos == string::npos)
            break;
        pos += 1;
        occurrence++;
    }
    string new_prefix = path.substr (0, pos);
    if (new_prefix.back () != '/')
        new_prefix.push_back ('/');
    prefix = std::move (new_prefix);
    return 0;
}

//
// This id is local to its ancestor level defined by "scope"
// scope=0: id is local to its parent
// scope=1: id is local to its grand parent
// For example, in rack[1]->node[18]->socket[2]->core[8] configuration,
// if scope is 1, the id space of a core resource is local to
// the node level instead of the socket level.
// So, 16 cores in each node will have 0-15, instead of repeating
// 0-7 and 0-7, which will be the case if the scope is 0.
//
int dfs_emitter_t::gen_id (gge_t e, const gg_t &recipe, int i, int sz, int j)
{
    int h = 0;
    int j_dim_wrap = 1;
    int scope = recipe[e].id_scope;

    if (scope < 0)
        return -1;
    else if (scope == 0)
        return recipe[e].id_start + i;

    if (scope > (int) m_hier_scales.size ())
        scope = m_hier_scales.size ();
    j_dim_wrap = 1;
    deque<int>::const_iterator iter;
    for (h = 0; h < scope; ++h)
        j_dim_wrap *= m_hier_scales[h];

    return recipe[e].id_start
              + (j % j_dim_wrap * sz * recipe[e].id_stride)
              + (i * recipe[e].id_stride);
}

edg_t dfs_emitter_t::raw_edge (vtx_t src_v, vtx_t tgt_v)
{
    edg_t e; // Unfortunately, BGL does not have null_edge ()
    bool inserted;
    out_edg_iterator_t ei, ee;
    resource_graph_db_t &db = *m_db_p;

    tie (ei, ee) = out_edges (src_v, db.resource_graph);
    for ( ; ei != ee; ++ei) {
        if (target (*ei, db.resource_graph) == tgt_v) {
            e = (*ei);
            return e;
        }
    }
    tie (e, inserted) = add_edge (src_v, tgt_v, db.resource_graph);
    if (!inserted) {
        m_err_msg += "error inserting a new edge:"
                        + db.resource_graph[src_v].name
                        + " -> "
                        + db.resource_graph[tgt_v].name
                        + "; ";
    }
    return e;
}

void dfs_emitter_t::emit_edges (gge_t ge, const gg_t &recipe,
                                vtx_t src_v, vtx_t tgt_v)
{
    resource_graph_db_t &db = *m_db_p;
    edg_t e = raw_edge (src_v, tgt_v);
    if (m_err_msg != "")
        return;
    db.resource_graph[e].idata.member_of[recipe[ge].e_subsystem]
        = recipe[ge].relation;
    db.resource_graph[e].name += ":" + recipe[ge].e_subsystem
                                     + "." + recipe[ge].relation;
    e = raw_edge (tgt_v, src_v);
    if (m_err_msg != "")
        return;
    db.resource_graph[e].idata.member_of[recipe[ge].e_subsystem]
        = recipe[ge].rrelation;
    db.resource_graph[e].name += ":" + recipe[ge].e_subsystem
                                     + "." + recipe[ge].rrelation;
}

vtx_t dfs_emitter_t::emit_vertex (ggv_t u, gge_t e, const gg_t &recipe,
                                  vtx_t src_v, int i, int sz, int j)
{
    resource_graph_db_t &db = *m_db_p;
    if (src_v == boost::graph_traits<resource_graph_t>::null_vertex ())
        if (db.roots.find (recipe[u].subsystem) != db.roots.end ())
            return db.roots[recipe[u].subsystem];

    vtx_t v = add_vertex (db.resource_graph);;
    string pref = "";
    string ssys = recipe[u].subsystem;
    int id = 0;

    if (src_v == boost::graph_traits<resource_graph_t>::null_vertex ()) {
        // ROOT!!
        db.roots[recipe[u].subsystem] = v;
        id = 0;
    } else {
        // TODO: should add_vertex really be called a second time?
        v = add_vertex (db.resource_graph);
        id = gen_id (e, recipe, i, sz, j);
        pref = db.resource_graph[src_v].paths[ssys];
    }

    string istr = (id != -1)? to_string (id) : "";
    db.resource_graph[v].type = recipe[u].type;
    db.resource_graph[v].basename = recipe[u].basename;
    db.resource_graph[v].size = recipe[u].size;
    db.resource_graph[v].schedule.plans = planner_new (0, INT64_MAX,
                                                       recipe[u].size,
                                                       recipe[u].type.c_str ());
    db.resource_graph[v].schedule.x_checker = planner_new (0, INT64_MAX,
                                                           X_CHECKER_NJOBS,
                                                           X_CHECKER_JOBS_STR);
    db.resource_graph[v].id = id;
    db.resource_graph[v].name = recipe[u].basename + istr;
    db.resource_graph[v].paths[ssys] = pref + "/" + db.resource_graph[v].name;
    db.resource_graph[v].idata.member_of[ssys] = "*";

    //
    // Indexing for fast look-up...
    //
    db.by_path[db.resource_graph[v].paths[ssys]].push_back (v);
    db.by_type[db.resource_graph[v].type].push_back (v);
    db.by_name[db.resource_graph[v].name].push_back (v);
    return v;
}


/********************************************************************************
 *                                                                              *
 *                          Public DFS Visitor Emitter                          *
 *                                                                              *
 ********************************************************************************/

dfs_emitter_t::dfs_emitter_t ()
{

}

dfs_emitter_t::dfs_emitter_t (resource_graph_db_t *d, resource_gen_spec_t *g)
{
    m_db_p = d;
    m_gspec_p = g;
}

dfs_emitter_t::dfs_emitter_t (const dfs_emitter_t &o)
{
    m_db_p = o.m_db_p;
    m_gspec_p = o.m_gspec_p;
    m_err_msg = o.m_err_msg;
}

dfs_emitter_t::~dfs_emitter_t()
{
    m_gen_src_vtx.clear ();
    m_hier_scales.clear ();
}

dfs_emitter_t &dfs_emitter_t::operator=(const dfs_emitter_t &o)
{
    m_db_p = o.m_db_p;
    m_gspec_p = o.m_gspec_p;
    m_err_msg = o.m_err_msg;
    return *this;
}

/*
 * Visitor method that is invoked on a tree-edge event
 * generated by depth_first_walk ()
 *
 * \param e              resource generator graph edge descriptor
 * \param recipe         resource generator recipe graph
 */
void dfs_emitter_t::tree_edge (gge_t e, const gg_t &recipe)
{
    vtx_t src_vtx, tgt_vtx;
    ggv_t src_ggv = source (e, recipe);
    ggv_t tgt_ggv = target (e, recipe);
    vector<vtx_t>::iterator src_it, tgt_it;
    resource_graph_db_t &db = *m_db_p;
    string in;
    int i = 0, j = 0;;

    if (recipe[src_ggv].root) {
        //! ROOT
        if (m_gen_src_vtx[src_ggv].empty ()) {
            vtx_t null_v = boost::graph_traits<resource_graph_t>::null_vertex ();
            m_gen_src_vtx[src_ggv].push_back (emit_vertex (src_ggv, e, recipe,
                                                           null_v, 0, 1, 0));
        }
    }

    m_gen_src_vtx[tgt_ggv] = vector<vtx_t>();

    switch (m_gspec_p->to_gen_method_t (recipe[e].gen_method)) {
    case MULTIPLY:
        for (src_it = m_gen_src_vtx[src_ggv].begin ();
             src_it != m_gen_src_vtx[src_ggv].end (); src_it++, j++) {

            src_vtx = *src_it;
            for (i = 0; i < recipe[e].multi_scale; ++i) {
                tgt_vtx = emit_vertex (tgt_ggv, e, recipe, src_vtx, i,
                                       recipe[e].multi_scale, j);
                emit_edges (e, recipe, src_vtx, tgt_vtx);
                // TODO: Next gen src vertex; where do you clear them?
                m_gen_src_vtx[tgt_ggv].push_back (tgt_vtx);
            }
        }
        m_hier_scales.push_front (recipe[e].multi_scale);
        break;

    case ASSOCIATE_IN:
        for (src_it = m_gen_src_vtx[src_ggv].begin ();
             src_it != m_gen_src_vtx[src_ggv].end (); src_it++) {

            src_vtx = *src_it;
            for (tgt_it = db.by_type[recipe[tgt_ggv].type].begin();
                 tgt_it != db.by_type[recipe[tgt_ggv].type].end(); tgt_it++) {
                tgt_vtx = (*tgt_it);
                db.resource_graph[tgt_vtx].paths[recipe[e].e_subsystem]
                    = db.resource_graph[src_vtx].paths[recipe[e].e_subsystem]
                          + "/" + db.resource_graph[tgt_vtx].name;
                db.resource_graph[tgt_vtx].idata.member_of[recipe[e].e_subsystem]
                    = "*";
                emit_edges (e, recipe, src_vtx, tgt_vtx);
                m_gen_src_vtx[tgt_ggv].push_back (tgt_vtx);
            }
        }
        break;

    case ASSOCIATE_BY_PATH_IN:
        in = recipe[e].as_tgt_subsystem;
        for (src_it = m_gen_src_vtx[src_ggv].begin ();
             src_it != m_gen_src_vtx[src_ggv].end (); src_it++) {

            src_vtx = *src_it;
            for (tgt_it = db.by_type[recipe[tgt_ggv].type].begin();
                 tgt_it != db.by_type[recipe[tgt_ggv].type].end(); tgt_it++) {
                string comp_pth1, comp_pth2;
                tgt_vtx = (*tgt_it);
                path_prefix (db.resource_graph[tgt_vtx].paths[in],
                             recipe[e].as_tgt_uplvl, comp_pth1);
                path_prefix (db.resource_graph[src_vtx].paths[in],
                             recipe[e].as_src_uplvl, comp_pth2);

                if (comp_pth1 != comp_pth2)
                    continue;

                db.resource_graph[tgt_vtx].paths[recipe[e].e_subsystem]
                    = db.resource_graph[src_vtx].paths[recipe[e].e_subsystem]
                          + "/" + db.resource_graph[tgt_vtx].name;
                db.resource_graph[tgt_vtx].idata.member_of[recipe[e].e_subsystem]
                    = "*";
                emit_edges (e, recipe, src_vtx, tgt_vtx);
                m_gen_src_vtx[tgt_ggv].push_back (tgt_vtx);
            }
        }
        break;

    case GEN_UNKNOWN:
    default:
        m_err_msg += "unknown generation method; ";
        break;
    }
}

/*! Visitor method that is invoked on a finish vertex by DFS visitor
 *
 *  \param u             resource generator graph vertex descriptor
 *  \param recipe        resource generator recipe graph
 */
void dfs_emitter_t::finish_vertex (ggv_t u, const gg_t &recipe)
{
    if (m_hier_scales.size())
        m_hier_scales.pop_front ();
}

/*! Return the error message. All error messages that
 *  encountered have been concatenated.
 *
 *  \return       error message
 */
const string &dfs_emitter_t::err_message () const
{
    return m_err_msg;
}


/********************************************************************************
 *                                                                              *
 *                   Public Resource Generator Interface                        *
 *                                                                              *
 ********************************************************************************/

resource_generator_t::resource_generator_t ()
{

}

resource_generator_t::~resource_generator_t ()
{

}

resource_generator_t::resource_generator_t (const resource_generator_t &o)
{
    m_gspec = o.m_gspec;
    m_err_msg = o.m_err_msg;
}

const resource_generator_t &resource_generator_t::operator= (
                                                    const resource_generator_t &o)
{
    m_gspec = o.m_gspec;
    m_err_msg = o.m_err_msg;
    return *this;
}

/*
 * Return an error message string. All error messages that
 * encountered have been concatenated.
 *
 * \return               error message string
 */
const std::string &resource_generator_t::err_message () const
{
    return m_err_msg;
}

/*
 * Read a subsystem spec graphml file and generate resource database
 *
 * \param sn     generator spec file in graphml
 * \param db     graph database consisting of resource graph and various indices
 * \return       0 on success; non-zero integer on an error
 */
int resource_generator_t::read_graphml (const string &fn, resource_graph_db_t &db)
{
    int rc = 0;
    if (m_gspec.read_graphml (fn) != 0) {
        m_err_msg += "error in reading " + fn + "; ";
        return -1;
    }

    //
    // depth_first_search on the generator recipe graph
    // with emitter visitor.
    //
    dfs_emitter_t emitter (&db, &m_gspec);
    depth_first_search (m_gspec.gen_graph (), boost::visitor (emitter));
    m_err_msg += emitter.err_message ();

    return (m_err_msg == "")? rc : -1;
}

ggv_t add_new_vertex(resource_graph_db_t &db, const ggv_t &parent,
                     int id, const string &subsys, const string &type,
                     const string &basename, int size, int rank=-1)
{
    ggv_t v = boost::add_vertex (db.resource_graph);

    // Set properties of the new vertex
    bool is_root = parent == boost::graph_traits<resource_graph_t>::null_vertex ();
    string istr = (id != -1)? to_string (id) : "";
    string prefix =  is_root ? "" : db.resource_graph[parent].paths[subsys];

    db.resource_graph[v].type = type;
    db.resource_graph[v].basename = basename;
    db.resource_graph[v].size = size;
    db.resource_graph[v].rank = rank;
    db.resource_graph[v].schedule.plans = planner_new (0, INT64_MAX,
                                                       size,
                                                       type.c_str ());
    db.resource_graph[v].schedule.x_checker = planner_new (0, INT64_MAX,
                                                           X_CHECKER_NJOBS,
                                                           X_CHECKER_JOBS_STR);
    db.resource_graph[v].id = id;
    db.resource_graph[v].name = basename + istr;
    db.resource_graph[v].paths[subsys] = prefix + "/" + db.resource_graph[v].name;
    db.resource_graph[v].idata.member_of[subsys] = "*";

    // Indexing for fast look-up
    db.by_path[db.resource_graph[v].paths[subsys]].push_back (v);
    db.by_type[db.resource_graph[v].type].push_back (v);
    db.by_name[db.resource_graph[v].name].push_back (v);
    return v;
}

void walk_hwloc (const hwloc_obj_t obj, const ggv_t parent, int rank, resource_graph_db_t &db)
{
    bool supported_resource = true;
    std::string type, basename;
    int id = obj->logical_index;
    unsigned int size = 1;

    switch(obj->type) {
    case HWLOC_OBJ_MACHINE: {
        // TODO: add signature to support multiple ranks per node
        const char *hwloc_name = hwloc_obj_get_info_by_name (obj, "HostName");
        if (!hwloc_name) {
            supported_resource = false;
            break;
        }
        type = "node";
        basename = hwloc_name;
        id = -1; // TODO: is this the right thing to do?
        break;
    }
    case HWLOC_OBJ_GROUP: {
        type = "group";
        basename = type;
        break;
    }
    case HWLOC_OBJ_NUMANODE: {
        type = "numanode";
        basename = type;
        break;
    }
    case HWLOC_OBJ_PACKAGE: {
        type = "socket";
        basename = type;
        break;
    }
#if HWLOC_API_VERSION < 0x00020000
    case HWLOC_OBJ_CACHE: {
        type = "cache";
        basename = "L" + to_string (obj->attr->cache.depth) + type;
        size = obj->attr->cache.size / 1024;
        break;
    }
#else
    case HWLOC_OBJ_L1CACHE: {
        type = "cache";
        basename = "L1" + type;
        size = obj->attr->cache.size / 1024;
        break;
    }
    case HWLOC_OBJ_L2CACHE: {
        type = "cache";
        basename = "L2" + type;
        size = obj->attr->cache.size / 1024;
        break;
    }
    case HWLOC_OBJ_L3CACHE: {
        type = "cache";
        basename = "L3" + type;
        size = obj->attr->cache.size / 1024;
        break;
    }
#endif
    case HWLOC_OBJ_CORE: {
        type = "core";
        basename = type;
        break;
    }
    case HWLOC_OBJ_PU: {
        type = "pu";
        basename = type;
        break;
    }
    case HWLOC_OBJ_OS_DEVICE: {
        if (obj->attr && obj->attr->osdev.type == HWLOC_OBJ_OSDEV_COPROC) {
            /* hwloc doesn't provide the logical index only amongst CoProc
             * devices so we parse this info from the name until hwloc provide
             * better support.
             */
            if (strncmp(obj->name, "cuda", 4) == 0)
                id = atoi (obj->name + 4);
            else if (strncmp(obj->name, "opencl", 6) == 0)
                id = atoi (obj->name + 6);
            type = "gpu";
            basename = type;
        } else {
            supported_resource = false;
        }
        break;
    }
    default: {
        supported_resource = false;
        break;
    }
    }

    // A valid ancestor vertex to pass to the recursive call
    ggv_t valid_ancestor;
    if (!supported_resource) {
        valid_ancestor = parent;
    } else {
        const string subsys = "containment";
        ggv_t v = add_new_vertex(db, parent, id, subsys, type, basename, size, rank);
        valid_ancestor = v;

        // Create edge between parent/child
        if (parent == boost::graph_traits<resource_graph_t>::null_vertex ()) {
            // is root
            db.roots[subsys] = v;
        } else {
            string relation = "contains";
            string rev_relation = "in";
            edg_t e;
            bool inserted; // set to false when we try and insert a parallel edge

            tie (e, inserted) = add_edge(parent, v, db.resource_graph);
            db.resource_graph[e].idata.member_of[subsys] = relation;
            db.resource_graph[e].name += ":" + subsys + "." + relation;

            tie (e, inserted) = add_edge(v, parent, db.resource_graph);
            db.resource_graph[e].idata.member_of[subsys] = rev_relation;
            db.resource_graph[e].name += ":" + subsys + "." + rev_relation;
        }
    }

    for (unsigned int i = 0; i < obj->arity; i++) {
        walk_hwloc (obj->children[i], valid_ancestor, rank, db);
    }
}

int check_hwloc_version (string &m_err_msg) {
    unsigned int hwloc_version = hwloc_get_api_version ();

    if ((hwloc_version >> 16) != (HWLOC_API_VERSION >> 16)) {
        stringstream msg;
        msg << "Compiled for hwloc API 0x"
            << std::hex << HWLOC_API_VERSION
            << " but running on library API 0x"
            << hwloc_version << "; ";
        m_err_msg += msg.str ();
        errno = EINVAL;
        return -1;
    }

    return 0;
}

ggv_t resource_generator_t::create_cluster_vertex (resource_graph_db_t &db)
{
    // generate cluster root vertex
    const string subsys = "containment";
    ggv_t v = add_new_vertex (db, boost::graph_traits<resource_graph_t>::null_vertex (),
                              0, subsys, "cluster", "cluster", 1);
    db.roots[subsys] = v;

    return v;
}

int resource_generator_t::read_ranked_hwloc_xml (const char *hwloc_xml, int rank,
                                                 const ggv_t &root_vertex, resource_graph_db_t &db)
{
    if (check_hwloc_version (m_err_msg) < 0) {
        return -1;
    }

    size_t len = strlen (hwloc_xml);

    hwloc_topology_t topo;
    if ((hwloc_topology_init (&topo) != 0) ||
        (hwloc_topology_set_flags (topo, HWLOC_TOPOLOGY_FLAG_IO_DEVICES) != 0) ||
        (hwloc_topology_set_xmlbuffer (topo, hwloc_xml, len) != 0) ||
        (hwloc_topology_load (topo) != 0))
        {
            hwloc_topology_destroy (topo);
            m_err_msg += "Failed to load hwloc xml from rank " + to_string (rank) + "; ";
            return -1;
        }

    hwloc_obj_t hwloc_root = hwloc_get_root_obj (topo);
    walk_hwloc (hwloc_root, root_vertex, rank, db);

    hwloc_topology_destroy (topo);
    return 0;
}

/*
 * Read a subsystem spec hwloc xml file and generate resource database
 *
 * \param ifn    filename of hwloc xml
 * \param db     graph database consisting of resource graph and various indices
 * \return       0 on success; non-zero integer on an error
 */
int resource_generator_t::read_hwloc_xml_file (const char *ifn,
                                               resource_graph_db_t &db)
{
    if (check_hwloc_version (m_err_msg) < 0) {
        return -1;
    }

    ggv_t cluster_vertex = create_cluster_vertex (db);
    std::ifstream infile (ifn);
    if (!infile.good ())
        return -1;

    std::string xml_str ((std::istreambuf_iterator<char> (infile)),
                         std::istreambuf_iterator<char> ());
    read_ranked_hwloc_xml (xml_str.c_str(), -1, cluster_vertex, db);

    return 0;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
