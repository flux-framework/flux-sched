/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef RESOURCE_GRAPH_HPP
#define RESOURCE_GRAPH_HPP

#include <boost/config.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/graphml.hpp>
#include "resource/schema/resource_data.hpp"

namespace Flux {
namespace resource_model {

enum class emit_format_t { GRAPHVIZ_DOT, GRAPH_ML, };

using pinfra_t = pool_infra_t resource_pool_t::*;
using pname_t  = std::string resource_t::*;
using rname_t  = std::string resource_relation_t::*;
using rinfra_t = relation_infra_t resource_relation_t::*;
using resource_graph_t = boost::adjacency_list<boost::vecS,
                                               boost::vecS,
                                               boost::directedS,
                                               resource_pool_t,
                                               resource_relation_t,
                                               std::string>;

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
    subsystem_selector_t (inframap &im, const multi_subsystemsS &sel,
                          int subsys_size)
    {
        // must be lightweight -- e.g., bundled property map.
        m_imap = im;
        m_selector = sel;
        m_single_subsystem = (subsys_size == 1) ? true : false;
    }
    bool operator () (const graph_entity &ent) const {
        // Short circuit for single subsystem. This will be a common case.
        // TODO: make systems ints or enums for faster comparison and search.
        if (m_single_subsystem)
           return true;
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
    bool m_single_subsystem;
};

using vtx_infra_map_t = boost::property_map<resource_graph_t, pinfra_t>::type;
using edg_infra_map_t = boost::property_map<resource_graph_t, rinfra_t>::type;
using vtx_t = boost::graph_traits<resource_graph_t>::vertex_descriptor;
using edg_t = boost::graph_traits<resource_graph_t>::edge_descriptor;
using vtx_iterator_t = boost::graph_traits<resource_graph_t>::vertex_iterator;
using edg_iterator_t = boost::graph_traits<resource_graph_t>::edge_iterator;
using out_edg_iterator_t = boost::graph_traits<resource_graph_t>::out_edge_iterator;
using f_resource_graph_t = boost::filtered_graph<resource_graph_t,
                                                 subsystem_selector_t<edg_t,
                                                               edg_infra_map_t>,
                                                 subsystem_selector_t<vtx_t,
                                                               vtx_infra_map_t>>;
using f_edg_infra_map_t = boost::property_map<f_resource_graph_t, rinfra_t>::type;
using f_vtx_infra_map_t = boost::property_map<f_resource_graph_t, pinfra_t>::type;
using f_res_name_map_t = boost::property_map<f_resource_graph_t, pname_t>::type;
using f_rel_name_map_t = boost::property_map<f_resource_graph_t, rname_t>::type;
using f_vtx_infra_map_t = boost::property_map<f_resource_graph_t, pinfra_t>::type;
using f_vtx_iterator_t = boost::graph_traits<f_resource_graph_t>::vertex_iterator;
using f_edg_iterator_t = boost::graph_traits<f_resource_graph_t>::edge_iterator;
using f_out_edg_iterator_t = boost::graph_traits<f_resource_graph_t>::out_edge_iterator;

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
