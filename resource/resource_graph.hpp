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

#ifndef RESOURCE_GRAPH_HPP
#define RESOURCE_GRAPH_HPP

#include <boost/config.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/graphml.hpp>
#include "resource_data.hpp"

namespace Flux {
namespace resource_model {

enum class emit_format_t { GRAPHVIZ_DOT, GRAPH_ML, };

typedef pool_infra_t resource_pool_t::* pinfra_t;
typedef std::string resource_pool_t::* pname_t;
typedef std::string resource_relation_t::* rname_t;
typedef relation_infra_t resource_relation_t::* rinfra_t;

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, resource_pool_t,
                       resource_relation_t, std::string>  resource_graph_t;

/*! For each chosen subsystem, a selector has std::set<string>.
 *  If edge/vertex's member_of[subsystem] matches with one
 *  of these strings, select that egde/vertex. Edge/vertex itself
 *  can also self-select. If its member_of[subsystem] has been
 *  annotated with '*', it is selected.
 */
template <typename graph_entity, typename inframap>
class subsystem_selector_t {
public:
    subsystem_selector_t () {}
    ~subsystem_selector_t () {}
    subsystem_selector_t (inframap &im, const multi_subsystemsS &sel)
    {
        // must be lightweight -- e.g., bundled property map.
        m_imap = im;
        m_selector = sel;
    }
    bool operator () (const graph_entity &ent) const {
        typedef typename boost::property_traits<inframap>::value_type infra_type;
        const infra_type &inf = get (m_imap, ent);
        const multi_subsystems_t &subsystems = inf.member_of;
        for (auto &kv : subsystems) {
            multi_subsystemsS::const_iterator i;
            i = m_selector.find (kv.first);
            if (i != m_selector.end ()) {
                if (kv.second == "*")
                    return true;
                else if (i->second.find (kv.second) != i->second.end ()
                         || i->second.find ("*") != i->second.end ())
                    return true;
            }
        }
        return false;
    }

private:
    multi_subsystemsS m_selector;
    inframap m_imap;
};

typedef boost::property_map<resource_graph_t, pinfra_t>::type    vtx_infra_map_t;
typedef boost::property_map<resource_graph_t, rinfra_t>::type    edg_infra_map_t;
typedef boost::graph_traits<resource_graph_t>::vertex_descriptor vtx_t;
typedef boost::graph_traits<resource_graph_t>::edge_descriptor   edg_t;
typedef boost::graph_traits<resource_graph_t>::vertex_iterator   vtx_iterator_t;
typedef boost::graph_traits<resource_graph_t>::edge_iterator     edg_iterator_t;
typedef boost::graph_traits<resource_graph_t>::out_edge_iterator out_edg_iterator_t;
typedef boost::filtered_graph<resource_graph_t,
                       subsystem_selector_t<edg_t, edg_infra_map_t>,
                       subsystem_selector_t<vtx_t, vtx_infra_map_t>>
                                                          f_resource_graph_t;
typedef boost::property_map<f_resource_graph_t, rinfra_t>::type  f_edg_infra_map_t;
typedef boost::property_map<f_resource_graph_t, pinfra_t>::type  f_vtx_infra_map_t;
typedef boost::property_map<f_resource_graph_t, pname_t>::type   f_res_name_map_t;
typedef boost::property_map<f_resource_graph_t, rname_t>::type   f_rel_name_map_t;
typedef boost::property_map<f_resource_graph_t, pinfra_t>::type  f_vtx_infra_map_t;
typedef boost::graph_traits<f_resource_graph_t>::vertex_iterator f_vtx_iterator_t;
typedef boost::graph_traits<f_resource_graph_t>::edge_iterator   f_edg_iterator_t;
typedef boost::graph_traits<f_resource_graph_t>::out_edge_iterator f_out_edg_iterator_t;

/*! Resource graph data store.
 *  Adjacency_list graph, roots of this graph and various indexing.
 */
struct resource_graph_db_t {
    resource_graph_t resource_graph;
    std::map<subsystem_t, vtx_t> roots;
    std::map<std::string, std::vector <vtx_t>> by_type;
    std::map<std::string, std::vector <vtx_t>> by_name;
    std::map<std::string, std::vector <vtx_t>> by_path;
};

template<class name_map, class graph_entity>
class label_writer_t {
public:
    label_writer_t (name_map &in_map): m (in_map) { }
    void operator()(std::ostream &out, const graph_entity ent) const {
        out << "[label=\"" << m[ent] << "\"]";
    }
private:
    name_map m;
};

class edg_label_writer_t {
public:
    edg_label_writer_t (f_edg_infra_map_t &idata, subsystem_t &s)
        : m_infra (idata), m_s (s) {}
    void operator()(std::ostream& out, const edg_t &e) const {
        multi_subsystems_t::iterator i = m_infra[e].member_of.find (m_s);
        if (i != m_infra[e].member_of.end ()) {
            out << "[label=\"" << i->second << "\"]";
        } else {
            i = m_infra[e].member_of.begin ();
            out << "[label=\"" << i->second << "\"]";
        }
    }
private:
    f_edg_infra_map_t m_infra;
    subsystem_t m_s;
};

} // namespace resource_model
} // namespace Flux

#endif // RESOURCE_GRAPH_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
