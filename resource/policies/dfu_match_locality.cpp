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
#include <config.h>
#endif
}

#include "resource/policies/dfu_match_locality.hpp"

namespace Flux {
namespace resource_model {

greater_interval_first_t::greater_interval_first_t ()
{

}

greater_interval_first_t::greater_interval_first_t (const std::string &name)
    : dfu_match_cb_t (name)
{

}

greater_interval_first_t::greater_interval_first_t (
    const greater_interval_first_t &o)
    : dfu_match_cb_t (o)
{

}

greater_interval_first_t &greater_interval_first_t::operator= (
    const greater_interval_first_t &o)
{
    dfu_match_cb_t::operator= (o);
    return *this;
}

greater_interval_first_t::~greater_interval_first_t ()
{

}

int greater_interval_first_t::dom_finish_graph (
    const subsystem_t &subsystem,
    const std::vector<Flux::Jobspec::Resource> &resources,
    const resource_graph_t &g, scoring_api_t &dfu)
{
    using namespace boost::icl;
    int score = MATCH_MET;
    fold::interval_greater comp;

    for (auto &resource : resources) {
        const std::string &type = resource.type;
        unsigned int qc = dfu.qualified_count (subsystem, type);
        unsigned int count = calc_count (resource, qc);
        if (count == 0) {
            score = MATCH_UNMET;
            break;
        }
        dfu.transform (subsystem, type,
                       boost::icl::inserter (comp.ivset, comp.ivset.end ()),
                       fold::to_interval);
        dfu.choose_accum_best_k (subsystem, type, count, comp);
    }
    dfu.set_overall_score (score);
    return (score == MATCH_MET)? 0 : -1;
}

int greater_interval_first_t::dom_finish_slot (
    const subsystem_t &subsystem,
    scoring_api_t &dfu)
{
    std::vector<std::string> types;
    dfu.resrc_types (subsystem, types);
    for (auto &type : types)
        dfu.choose_accum_all (subsystem, type);
    return 0;
}

int greater_interval_first_t::dom_finish_vtx (
    vtx_t u,
    const subsystem_t &subsystem,
    const std::vector<Flux::Jobspec::Resource> &resources,
    const resource_graph_t &g, scoring_api_t &dfu)
{
    int64_t score = MATCH_MET;
    int64_t overall;

    for (auto &resource : resources) {
        if (resource.type != g[u].type)
            continue;

        // jobspec resource type matches with the visiting vertex
        for (auto &c_resource : resource.with) {
            // test children resource count requirements
            const std::string &c_type = c_resource.type;
            unsigned int qc = dfu.qualified_count (subsystem, c_type);
            unsigned int count = calc_count (c_resource, qc);
            if (count == 0) {
                score = MATCH_UNMET;
                break;
            }
            dfu.choose_accum_best_k (subsystem, c_resource.type, count);
        }
    }

    overall = (score == MATCH_MET)? (score + g[u].id + 1) : score;
    dfu.set_overall_score (overall);
    decr ();
    return (score == MATCH_MET)? 0 : -1;
}

} // resource_model
} // Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
