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

#ifndef RESOURCE_GEN_SPEC_HPP
#define RESOURCE_GEN_SPEC_HPP

#include <string>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/graphml.hpp>
#include <boost/filesystem.hpp>

namespace Flux {
namespace resource_model {

enum gen_meth_t {
    MULTIPLY,
    ASSOCIATE_IN,
    ASSOCIATE_BY_PATH_IN,
    GEN_UNKNOWN
};

struct resource_pool_gen_t {
    int root;
    std::string type;
    std::string basename;
    long size;
    std::string unit;
    std::string subsystem;
};

struct relation_gen_t {
    std::string e_subsystem;
    std::string relation;
    std::string rrelation;
    int id_scope;
    int id_start;
    int id_stride;
    std::string gen_method;
    int multi_scale;
    std::string as_tgt_subsystem;
    int as_tgt_uplvl;
    int as_src_uplvl;
};

typedef boost::adjacency_list<
    boost::vecS,
    boost::vecS,
    boost::directedS,
    resource_pool_gen_t,
    relation_gen_t
> gg_t;

typedef std::string resource_pool_gen_t::* pgen_t;
typedef std::string relation_gen_t::* rgen_t;
typedef boost::graph_traits<gg_t>::vertex_descriptor ggv_t;
typedef boost::graph_traits<gg_t>::edge_descriptor gge_t;

typedef boost::property_map<gg_t, pgen_t>::type vtx_type_map_t;
typedef boost::property_map<gg_t, pgen_t>::type vtx_basename_map_t;
typedef boost::property_map<gg_t, long resource_pool_gen_t::* >::type vtx_size_map_t;
typedef boost::property_map<gg_t, pgen_t>::type vtx_unit_map_t;
typedef boost::property_map<gg_t, pgen_t>::type vtx_subsystem_map_t;
typedef boost::property_map<gg_t, rgen_t>::type edg_e_subsystem_map_t;
typedef boost::property_map<gg_t, rgen_t>::type edg_relation_map_t;
typedef boost::property_map<gg_t, rgen_t>::type edg_rrelation_map_t;
typedef boost::property_map<gg_t, rgen_t>::type edg_gen_method_map_t;
typedef boost::property_map<gg_t, rgen_t>::type edg_id_method_map_t;
typedef boost::property_map<gg_t, int relation_gen_t::* >::type edg_multi_scale_map_t;

class resource_gen_spec_t {
public:
    resource_gen_spec_t ();
    resource_gen_spec_t (const resource_gen_spec_t &o);
    const gg_t &gen_graph ();
    const gen_meth_t to_gen_method_t (const std::string &s) const;
    int read_graphml (const std::string &ifn);
    int write_graphviz (const std::string &ofn, bool simple=false);

private:
    void setup_dynamic_property (boost::dynamic_properties &dp, gg_t &g);
    gg_t g;
    boost::dynamic_properties dp;
};

} // namespace resource_model
} // namespace Flux

#endif // RESOURCE_GEN_SPEC_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
