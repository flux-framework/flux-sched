/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef RESOURCE_SPEC_GRUG_HPP
#define RESOURCE_SPEC_GRUG_HPP

#include <string>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/graphml.hpp>
#include <boost/filesystem.hpp>

namespace Flux {
namespace resource_model {

enum gen_meth_t { MULTIPLY, ASSOCIATE_IN, ASSOCIATE_BY_PATH_IN, GEN_UNKNOWN };

struct resource_pool_gen_t {
    int root;
    std::string type;
    std::string basename;
    int rank;
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

using gg_t = boost::
    adjacency_list<boost::vecS, boost::vecS, boost::directedS, resource_pool_gen_t, relation_gen_t>;
using pgen_t = std::string resource_pool_gen_t::*;
using rgen_t = std::string relation_gen_t::*;
using ggv_t = boost::graph_traits<gg_t>::vertex_descriptor;
using gge_t = boost::graph_traits<gg_t>::edge_descriptor;

using vtx_type_map_t = boost::property_map<gg_t, pgen_t>::type;
using vtx_basename_map_t = boost::property_map<gg_t, pgen_t>::type;
using vtx_size_map_t = boost::property_map<gg_t, long resource_pool_gen_t::*>::type;
using vtx_unit_map_t = boost::property_map<gg_t, pgen_t>::type;
using vtx_subsystem_map_t = boost::property_map<gg_t, pgen_t>::type;
using edg_e_subsystem_map_t = boost::property_map<gg_t, rgen_t>::type;
using edg_relation_map_t = boost::property_map<gg_t, rgen_t>::type;
using edg_rrelation_map_t = boost::property_map<gg_t, rgen_t>::type;
using edg_gen_method_map_t = boost::property_map<gg_t, rgen_t>::type;
using edg_id_method_map_t = boost::property_map<gg_t, rgen_t>::type;
using edg_multi_scale_map_t = boost::property_map<gg_t, int relation_gen_t::*>::type;

class resource_gen_spec_t {
   public:
    resource_gen_spec_t ();
    resource_gen_spec_t (const resource_gen_spec_t &o);
    const gg_t &gen_graph ();
    const gen_meth_t to_gen_method_t (const std::string &s) const;
    int read_graphml (const std::string &ifn);
    int read_graphml (std::istream &in);
    int write_graphviz (const std::string &ofn, bool simple = false);

   private:
    void setup_dynamic_property (boost::dynamic_properties &dp, gg_t &g);
    gg_t g;
    boost::dynamic_properties dp;
};

}  // namespace resource_model
}  // namespace Flux

#endif  // RESOURCE_SPEC_GRUG_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
