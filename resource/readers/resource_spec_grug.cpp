/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
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

#include <iostream>
#include <cstdint>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/graphml.hpp>
#include "resource/readers/resource_spec_grug.hpp"

using namespace Flux::resource_model;

struct str2enum_t {
    std::string str;
    int e;
};

str2enum_t str2genmeth[] = {{"MULTIPLY", MULTIPLY},
                            {"ASSOCIATE_IN", ASSOCIATE_IN},
                            {"ASSOCIATE_BY_PATH_IN", ASSOCIATE_BY_PATH_IN},
                            {"", GEN_UNKNOWN}};

template<class m1, class m2, class m3>
class gg_label_writer_t {
   public:
    gg_label_writer_t (m1 _n1, m2 _n2, m3 _n3) : n1 (_n1), n2 (_n2), n3 (_n3)
    {
    }
    template<class v_or_e>
    void operator() (std::ostream &out, const v_or_e &i) const
    {
        out << "[label=\"" << n1[i] << ":" << n2[i] << ":" << n3[i] << "\"]";
    }

   private:
    m1 n1;
    m2 n2;
    m3 n3;
};

template<class m1>
class gg_label_writer_sim_t {
   public:
    gg_label_writer_sim_t (m1 _n1) : n1 (_n1)
    {
    }
    template<class VertexOrEdge>
    void operator() (std::ostream &out, const VertexOrEdge &i) const
    {
        out << "[label=\"" << n1[i] << "\"]";
    }

   private:
    m1 n1;
};

////////////////////////////////////////////////////////////////////////////////
// Private Resource Generator Spec API
////////////////////////////////////////////////////////////////////////////////

void resource_gen_spec_t::setup_dynamic_property (boost::dynamic_properties &dp, gg_t &g)
{
    dp.property ("root", get (&resource_pool_gen_t::root, g));
    dp.property ("type", get (&resource_pool_gen_t::type, g));
    dp.property ("basename", get (&resource_pool_gen_t::basename, g));
    dp.property ("size", get (&resource_pool_gen_t::size, g));
    dp.property ("unit", get (&resource_pool_gen_t::unit, g));
    dp.property ("subsystem", get (&resource_pool_gen_t::subsystem, g));
    dp.property ("e_subsystem", get (&relation_gen_t::e_subsystem, g));
    dp.property ("relation", get (&relation_gen_t::relation, g));
    dp.property ("rrelation", get (&relation_gen_t::rrelation, g));
    dp.property ("gen_method", get (&relation_gen_t::gen_method, g));
    dp.property ("id_scope", get (&relation_gen_t::id_scope, g));
    dp.property ("id_start", get (&relation_gen_t::id_start, g));
    dp.property ("id_stride", get (&relation_gen_t::id_stride, g));
    dp.property ("multi_scale", get (&relation_gen_t::multi_scale, g));
    dp.property ("as_tgt_subsystem", get (&relation_gen_t::as_tgt_subsystem, g));
    dp.property ("as_tgt_uplvl", get (&relation_gen_t::as_tgt_uplvl, g));
    dp.property ("as_src_uplvl", get (&relation_gen_t::as_src_uplvl, g));
}

////////////////////////////////////////////////////////////////////////////////
// Public Resource Generator Spec API
////////////////////////////////////////////////////////////////////////////////

resource_gen_spec_t::resource_gen_spec_t ()
{
    setup_dynamic_property (dp, g);
}

resource_gen_spec_t::resource_gen_spec_t (const resource_gen_spec_t &o)
{
    g = o.g;
    dp = o.dp;
}

const gg_t &resource_gen_spec_t::gen_graph ()
{
    return g;
};

/*! Return gen_meth_t enum value corresponding to the string passed in.
 *
 *  \param s             gen_method string from graphml spec
 *  \return              0 on success; -1 otherwise
 */
const gen_meth_t resource_gen_spec_t::to_gen_method_t (const std::string &s) const
{
    int i;
    for (i = 0; str2genmeth[i].str != ""; ++i)
        if (str2genmeth[i].str == s)
            return (gen_meth_t)str2genmeth[i].e;
    return (gen_meth_t)str2genmeth[i].e;
}

/*! Load resource generator recipe graph from a graphml file
 *
 *  \param ifn           resource generator recipe file in graphml
 *  \return              0 on success; -1 otherwise
 */
int resource_gen_spec_t::read_graphml (const std::string &ifn)
{
    int rc = 0;
    std::ifstream in_file (ifn.c_str ());
    if (!in_file.good ())
        return -1;
    rc = read_graphml (in_file);
    in_file.close ();
    return rc;
}

/*! Load resource generator recipe graph from an input stream
 *
 *  \param in            input stream for GRUG graphml
 *  \return              0 on success; -1 otherwise
 */
int resource_gen_spec_t::read_graphml (std::istream &in)
{
    int rc = 0;
    try {
        boost::read_graphml (in, g, dp);
    } catch (boost::exception &e) {
        errno = EINVAL;
        rc = -1;
    }
    return rc;
}

/*! Write the resource generator recipe graph in Graphviz dot format
 *
 * \param ofn            output file name
 * \param simple         if false, output will have more detailed info
 * \return               0 on success; -1 otherwise
 */
int resource_gen_spec_t::write_graphviz (const std::string &ofn, bool simple)
{
    int rc = 0;
    std::fstream out_file (ofn, std::fstream::out);
    try {
        vtx_basename_map_t v_bn_map = get (&resource_pool_gen_t::basename, g);
        vtx_size_map_t v_sz_map = get (&resource_pool_gen_t::size, g);
        // vtx_unit_map_t v_ut_map = get (&resource_pool_gen_t::unit, g);
        vtx_subsystem_map_t v_ss_map = get (&resource_pool_gen_t::subsystem, g);
        edg_relation_map_t e_rel_map = get (&relation_gen_t::relation, g);
        edg_gen_method_map_t e_gen_map = get (&relation_gen_t::gen_method, g);
        edg_multi_scale_map_t e_ms_map = get (&relation_gen_t::multi_scale, g);
        if (!simple) {
            gg_label_writer_t<vtx_subsystem_map_t, vtx_basename_map_t, vtx_size_map_t>
                vwr (v_ss_map, v_bn_map, v_sz_map);
            gg_label_writer_t<edg_relation_map_t, edg_gen_method_map_t, edg_multi_scale_map_t>
                ewr (e_rel_map, e_gen_map, e_ms_map);
            boost::write_graphviz (out_file, g, vwr, ewr);
        } else {
            gg_label_writer_sim_t<vtx_basename_map_t> vwr (v_bn_map);
            gg_label_writer_sim_t<edg_relation_map_t> ewr (e_rel_map);
            boost::write_graphviz (out_file, g, vwr, ewr);
        }
    } catch (boost::graph_exception &e) {
        rc = -1;
    }
    out_file.close ();
    return rc;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
