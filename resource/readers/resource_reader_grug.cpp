/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
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
#include <deque>
#include <vector>
#include <cstdint>
#include <boost/algorithm/string.hpp>
#include "resource/readers/resource_reader_grug.hpp"
#include "resource/store/resource_graph_store.hpp"
#include "resource/planner/c/planner.h"

using namespace Flux::resource_model;

/*! Note that this class must be copy-constructible
 * required by the concept of the depth first search
 * visitor. It must be lightweight.
 */
class dfs_emitter_t : public boost::default_dfs_visitor {
   public:
    dfs_emitter_t ();
    dfs_emitter_t (resource_graph_t *g_p, resource_graph_metadata_t *gm_p, resource_gen_spec_t *g);
    dfs_emitter_t (const dfs_emitter_t &o);
    dfs_emitter_t &operator= (const dfs_emitter_t &o);
    ~dfs_emitter_t ();

    void tree_edge (gge_t e, const gg_t &recipe);
    void finish_vertex (ggv_t u, const gg_t &recipe);
    const std::string &err_message () const;

    void set_rank (int rank);
    int get_rank ();

   private:
    vtx_t emit_vertex (ggv_t u, gge_t e, const gg_t &recipe, vtx_t src_v, int i, int sz, int j);
    int raw_edge (vtx_t src_v, vtx_t tgt_v, edg_t &e);
    void emit_edges (gge_t e, const gg_t &recipe, vtx_t src_v, vtx_t tgt_v);
    int path_prefix (const std::string &pth, int upl, std::string &pref);
    int gen_id (gge_t e, const gg_t &recipe, int i, int sz, int j);

    std::map<ggv_t, std::vector<vtx_t>> m_gen_src_vtx;
    std::deque<int> m_hier_scales;
    resource_graph_t *m_g_p = NULL;
    resource_graph_metadata_t *m_gm_p = NULL;
    resource_gen_spec_t *m_gspec_p = NULL;
    int m_rank = -1;
    std::string m_err_msg = "";
};

////////////////////////////////////////////////////////////////////////////////
// Private DFS Visitor Emitter API
////////////////////////////////////////////////////////////////////////////////

int dfs_emitter_t::path_prefix (const std::string &path, int uplevel, std::string &prefix)
{
    size_t pos = 0;
    unsigned int occurrence = 0;
    auto num_slashes = count (path.begin (), path.end (), '/');
    if (uplevel >= num_slashes)
        return -1;
    while (occurrence != (num_slashes - uplevel + 1)) {
        pos = path.find ("/", pos);
        if (pos == std::string::npos)
            break;
        pos += 1;
        occurrence++;
    }
    std::string new_prefix = path.substr (0, pos);
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

    if (scope > (int)m_hier_scales.size ())
        scope = m_hier_scales.size ();
    j_dim_wrap = 1;
    std::deque<int>::const_iterator iter;
    for (h = 0; h < scope; ++h)
        j_dim_wrap *= m_hier_scales[h];

    return recipe[e].id_start + (j % j_dim_wrap * sz * recipe[e].id_stride)
           + (i * recipe[e].id_stride);
}

int dfs_emitter_t::raw_edge (vtx_t src_v, vtx_t tgt_v, edg_t &e)
{
    bool inserted;
    out_edg_iterator_t ei, ee;
    resource_graph_t &g = *m_g_p;

    tie (ei, ee) = out_edges (src_v, g);
    for (; ei != ee; ++ei) {
        if (target (*ei, g) == tgt_v) {
            e = (*ei);
            return 0;
        }
    }
    tie (e, inserted) = add_edge (src_v, tgt_v, g);
    if (!inserted) {
        errno = ENOMEM;
        m_err_msg += "error inserting a new edge: " + g[src_v].name + " -> " + g[tgt_v].name + "; ";
        return -1;
    }
    // add this edge to by_outedges metadata
    auto iter = m_gm_p->by_outedges.find (src_v);
    if (iter == m_gm_p->by_outedges.end ()) {
        auto ret = m_gm_p->by_outedges.insert (
            std::make_pair (src_v,
                            std::map<std::pair<uint64_t, int64_t>,
                                     edg_t,
                                     std::greater<std::pair<uint64_t, int64_t>>> ()));
        if (!ret.second) {
            errno = ENOMEM;
            m_err_msg += "error creating out-edge metadata map: " + g[src_v].name + " -> "
                         + g[tgt_v].name + "; ";
            return -1;
        }
        iter = m_gm_p->by_outedges.find (src_v);
    }
    // Use a temporary for readability
    std::pair<uint64_t, int64_t> key = std::make_pair (g[e].idata.get_weight (), g[tgt_v].uniq_id);
    auto ret = iter->second.insert (std::make_pair (key, e));
    if (!ret.second) {
        errno = ENOMEM;
        m_err_msg += "error inserting an edge into out-edge metadata map:" + g[src_v].name + " -> "
                     + g[tgt_v].name + "; ";
        return -1;
    }
    return 0;
}

void dfs_emitter_t::emit_edges (gge_t ge, const gg_t &recipe, vtx_t src_v, vtx_t tgt_v)
{
    edg_t e;
    int rc = 0;
    resource_graph_t &g = *m_g_p;
    if ((rc = raw_edge (src_v, tgt_v, e)) < 0)
        return;
    g[e].idata.member_of[recipe[ge].e_subsystem] = true;
    g[e].subsystem = recipe[ge].e_subsystem;
}

vtx_t dfs_emitter_t::emit_vertex (ggv_t u,
                                  gge_t e,
                                  const gg_t &recipe,
                                  vtx_t src_v,
                                  int i,
                                  int sz,
                                  int j)
{
    resource_graph_t &g = *m_g_p;
    resource_graph_metadata_t &m = *m_gm_p;
    if (src_v == boost::graph_traits<resource_graph_t>::null_vertex ())
        if (m.roots.find (recipe[u].subsystem) != m.roots.end ())
            return (m.roots)[recipe[u].subsystem];

    vtx_t v = add_vertex (g);
    ;
    std::string pref = "";
    subsystem_t ssys = recipe[u].subsystem;
    int id = 0;

    if (src_v == boost::graph_traits<resource_graph_t>::null_vertex ()) {
        // ROOT vertex of graph
        m.roots.emplace (recipe[u].subsystem, v);
        m.v_rt_edges.emplace (recipe[u].subsystem, relation_infra_t ());
        id = 0;
    } else {
        id = gen_id (e, recipe, i, sz, j);
        pref = g[src_v].paths[ssys];
    }

    std::string istr = (id != -1) ? std::to_string (id) : "";
    g[v].type = recipe[u].type;
    g[v].basename = recipe[u].basename;
    g[v].size = recipe[u].size;
    g[v].unit = recipe[u].unit;
    g[v].schedule.plans = planner_new (0, INT64_MAX, recipe[u].size, recipe[u].type.c_str ());
    g[v].idata.x_checker = planner_new (0, INT64_MAX, X_CHECKER_NJOBS, X_CHECKER_JOBS_STR);
    g[v].id = id;
    g[v].name = recipe[u].basename + istr;
    g[v].paths[ssys] = pref + "/" + g[v].name;
    g[v].idata.member_of[ssys] = "*";
    g[v].uniq_id = v;
    g[v].rank = m_rank;

    //
    // Indexing for fast look-up...
    //
    m.by_path[g[v].paths[ssys]].push_back (v);
    m.by_type[g[v].type].push_back (v);
    m.by_name[g[v].name].push_back (v);
    m.by_rank[m_rank].push_back (v);
    return v;
}

////////////////////////////////////////////////////////////////////////////////
// Public DFS Visitor Emitter
////////////////////////////////////////////////////////////////////////////////

dfs_emitter_t::dfs_emitter_t ()
{
}

dfs_emitter_t::dfs_emitter_t (resource_graph_t *g_p,
                              resource_graph_metadata_t *gm_p,
                              resource_gen_spec_t *g)
{
    m_g_p = g_p;
    m_gm_p = gm_p;
    m_gspec_p = g;
}

dfs_emitter_t::dfs_emitter_t (const dfs_emitter_t &o)
{
    m_g_p = o.m_g_p;
    m_gm_p = o.m_gm_p;
    m_gspec_p = o.m_gspec_p;
    m_err_msg = o.m_err_msg;
}

dfs_emitter_t::~dfs_emitter_t ()
{
    m_gen_src_vtx.clear ();
    m_hier_scales.clear ();
}

dfs_emitter_t &dfs_emitter_t::operator= (const dfs_emitter_t &o)
{
    m_g_p = o.m_g_p;
    m_gm_p = o.m_gm_p;
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
    std::vector<vtx_t>::iterator src_it, tgt_it;
    resource_graph_t &g = *m_g_p;
    resource_graph_metadata_t &m = *m_gm_p;
    int i = 0, j = 0;
    ;

    if (recipe[src_ggv].root) {
        //! ROOT
        if (m_gen_src_vtx[src_ggv].empty ()) {
            vtx_t null_v = boost::graph_traits<resource_graph_t>::null_vertex ();
            m_gen_src_vtx[src_ggv].push_back (emit_vertex (src_ggv, e, recipe, null_v, 0, 1, 0));
        }
    }

    m_gen_src_vtx[tgt_ggv] = std::vector<vtx_t> ();

    switch (m_gspec_p->to_gen_method_t (recipe[e].gen_method)) {
        case MULTIPLY:
            for (src_it = m_gen_src_vtx[src_ggv].begin (); src_it != m_gen_src_vtx[src_ggv].end ();
                 src_it++, j++) {
                src_vtx = *src_it;
                for (i = 0; i < recipe[e].multi_scale; ++i) {
                    tgt_vtx =
                        emit_vertex (tgt_ggv, e, recipe, src_vtx, i, recipe[e].multi_scale, j);
                    emit_edges (e, recipe, src_vtx, tgt_vtx);
                    // TODO: Next gen src vertex; where do you clear them?
                    m_gen_src_vtx[tgt_ggv].push_back (tgt_vtx);
                }
            }
            m_hier_scales.push_front (recipe[e].multi_scale);
            break;

        case ASSOCIATE_IN:
            for (src_it = m_gen_src_vtx[src_ggv].begin (); src_it != m_gen_src_vtx[src_ggv].end ();
                 src_it++) {
                src_vtx = *src_it;
                for (tgt_it = m.by_type[recipe[tgt_ggv].type].begin ();
                     tgt_it != m.by_type[recipe[tgt_ggv].type].end ();
                     tgt_it++) {
                    tgt_vtx = (*tgt_it);
                    g[tgt_vtx].paths[recipe[e].e_subsystem] =
                        g[src_vtx].paths[recipe[e].e_subsystem] + "/" + g[tgt_vtx].name;
                    const std::string &tp = g[tgt_vtx].paths[recipe[e].e_subsystem];
                    m.by_path[tp].push_back (tgt_vtx);
                    g[tgt_vtx].idata.member_of[recipe[e].e_subsystem] = "*";
                    emit_edges (e, recipe, src_vtx, tgt_vtx);
                    m_gen_src_vtx[tgt_ggv].push_back (tgt_vtx);
                }
            }
            break;

        case ASSOCIATE_BY_PATH_IN: {
            subsystem_t in = recipe[e].as_tgt_subsystem;
            for (src_it = m_gen_src_vtx[src_ggv].begin (); src_it != m_gen_src_vtx[src_ggv].end ();
                 src_it++) {
                src_vtx = *src_it;
                for (tgt_it = m.by_type[recipe[tgt_ggv].type].begin ();
                     tgt_it != m.by_type[recipe[tgt_ggv].type].end ();
                     tgt_it++) {
                    std::string comp_pth1, comp_pth2;
                    tgt_vtx = (*tgt_it);
                    path_prefix (g[tgt_vtx].paths[in], recipe[e].as_tgt_uplvl, comp_pth1);
                    path_prefix (g[src_vtx].paths[in], recipe[e].as_src_uplvl, comp_pth2);

                    if (comp_pth1 != comp_pth2)
                        continue;

                    g[tgt_vtx].paths[recipe[e].e_subsystem] =
                        g[src_vtx].paths[recipe[e].e_subsystem] + "/" + g[tgt_vtx].name;
                    const std::string &tp = g[tgt_vtx].paths[recipe[e].e_subsystem];
                    m.by_path[tp].push_back (tgt_vtx);
                    g[tgt_vtx].idata.member_of[recipe[e].e_subsystem] = "*";
                    emit_edges (e, recipe, src_vtx, tgt_vtx);
                    m_gen_src_vtx[tgt_ggv].push_back (tgt_vtx);
                }
            }
            break;
        }

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
    if (m_hier_scales.size ())
        m_hier_scales.pop_front ();
}

/*! Return the error message. All error messages that
 *  encountered have been concatenated.
 *
 *  \return       error message
 */
const std::string &dfs_emitter_t::err_message () const
{
    return m_err_msg;
}

/*! Set rank whose value will be used to set the rank field
 *  of all of the generated vertices.
 */
void dfs_emitter_t::set_rank (int rank)
{
    m_rank = rank;
}

/*! Return rank whose value will be used to set the rank field
 *  of all of the generated vertices.
 *
 *  \return       rank
 */
int dfs_emitter_t::get_rank ()
{
    return m_rank;
}

////////////////////////////////////////////////////////////////////////////////
// Public GRUG Resource Reader Interface
////////////////////////////////////////////////////////////////////////////////

resource_reader_grug_t::~resource_reader_grug_t ()
{
}

int resource_reader_grug_t::unpack (resource_graph_t &g,
                                    resource_graph_metadata_t &m,
                                    const std::string &str,
                                    int rank)

{
    int rc = 0;
    std::istringstream in;
    in.str (str);

    if (m_gspec.read_graphml (in) != 0) {
        errno = EINVAL;
        m_err_msg += "error in reading grug string; ";
        return -1;
    }

    //
    // depth_first_search on the generator recipe graph
    // with emitter visitor.
    //
    dfs_emitter_t emitter (&g, &m, &m_gspec);
    emitter.set_rank (rank);
    depth_first_search (m_gspec.gen_graph (), boost::visitor (emitter));
    m_err_msg += emitter.err_message ();

    return (m_err_msg == "") ? rc : -1;
}

int resource_reader_grug_t::unpack_at (resource_graph_t &g,
                                       resource_graph_metadata_t &m,
                                       vtx_t &vtx,
                                       const std::string &str,
                                       int rank)
{
    errno = ENOTSUP;  // GRUG reader does not support unpack_at
    return -1;
}

int resource_reader_grug_t::update (resource_graph_t &g,
                                    resource_graph_metadata_t &m,
                                    const std::string &str,
                                    int64_t jobid,
                                    int64_t at,
                                    uint64_t dur,
                                    bool rsv,
                                    uint64_t token)
{
    errno = ENOTSUP;  // GRUG reader currently does not support update
    return -1;
}

int resource_reader_grug_t::partial_cancel (resource_graph_t &g,
                                            resource_graph_metadata_t &m,
                                            modify_data_t &mod_data,
                                            const std::string &R,
                                            int64_t jobid)
{
    errno = ENOTSUP;  // GRUG reader does not support partial cancel
    return -1;
}

bool resource_reader_grug_t::is_allowlist_supported ()
{
    return false;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
