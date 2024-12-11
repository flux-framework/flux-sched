/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#include "policies/base/dfu_match_cb.hpp"
extern "C" {
#if HAVE_CONFIG_H
#include <config.h>
#endif
}

#include "resource/policies/dfu_match_var_aware.hpp"

namespace Flux {
namespace resource_model {

var_aware_t::var_aware_t ()
{
}

var_aware_t::var_aware_t (const std::string &name) : dfu_match_cb_t (name)
{
}

var_aware_t::var_aware_t (const var_aware_t &o) : dfu_match_cb_t (o)
{
}

var_aware_t &var_aware_t::operator= (const var_aware_t &o)
{
    dfu_match_cb_t::operator= (o);
    return *this;
}

var_aware_t::~var_aware_t ()
{
}

int var_aware_t::dom_finish_graph (subsystem_t subsystem,
                                   const std::vector<Flux::Jobspec::Resource> &resources,
                                   const resource_graph_t &g,
                                   scoring_api_t &dfu)
{
    int score = MATCH_MET;
    fold::less comp;

    for (auto &resource : resources) {
        unsigned int qc = dfu.qualified_count (subsystem, resource.type);
        unsigned int count = calc_count (resource, qc);
        if (count == 0) {
            score = MATCH_UNMET;
            break;
        }
        dfu.choose_accum_best_k (subsystem, resource.type, count, comp);
    }
    dfu.set_overall_score (score);
    return (score == MATCH_MET) ? 0 : -1;
}

int var_aware_t::dom_finish_slot (subsystem_t subsystem, scoring_api_t &dfu)
{
    std::vector<resource_type_t> types;
    dfu.resrc_types (subsystem, types);
    for (auto &type : types)
        dfu.choose_accum_all (subsystem, type);
    return 0;
}

int var_aware_t::dom_finish_vtx (vtx_t u,
                                 subsystem_t subsystem,
                                 const std::vector<Flux::Jobspec::Resource> &resources,
                                 const resource_graph_t &g,
                                 scoring_api_t &dfu,
                                 traverser_match_kind_t sm)
{
    int64_t score = MATCH_MET;
    int64_t overall;
    fold::less comp;

    /* Default value for worst-performing-class assumed as 9999. */
    int64_t perf_class = 9999;

    for (auto &resource : resources) {
        if (resource.type != g[u].type)
            continue;

        // jobspec resource type matches with the visiting vertex
        for (auto &c_resource : resource.with) {
            // test children resource count requirements
            unsigned int qc = dfu.qualified_count (subsystem, c_resource.type);
            unsigned int count = calc_count (c_resource, qc);
            if (count == 0) {
                score = MATCH_UNMET;
                break;
            }
            dfu.choose_accum_best_k (subsystem, c_resource.type, count, comp);
        }
    }

    if (score == MATCH_MET) {
        try {
            perf_class = std::stoi (g[u].properties.at ("perf_class"));
        } catch (const std::out_of_range &oor) {
            perf_class = 9999;
        } catch (const std::invalid_argument &ia) {
            perf_class = 9999;
        }
        /* Ensure we don't have negative or more than
         * 9999 performance classes so we have clear boundaries. */
        if (perf_class < 0 || perf_class > 9999) {
            perf_class = 9999;
        }
    }

    overall = (score == MATCH_MET) ? (score + perf_class) : score;
    dfu.set_overall_score (overall);
    decr ();
    return (score == MATCH_MET) ? 0 : -1;
}

}  // namespace resource_model
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
