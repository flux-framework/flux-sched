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
#include "schema/data_std.hpp"
extern "C" {
#if HAVE_CONFIG_H
#include <config.h>
#endif
}

#include "resource/policies/dfu_match_cosched_aware.hpp"

namespace Flux {
namespace resource_model {

cosched_aware_t::cosched_aware_t ()
{
}

cosched_aware_t::cosched_aware_t (const std::string &name) : dfu_match_cb_t (name)
{
}

cosched_aware_t::cosched_aware_t (const cosched_aware_t &o) : dfu_match_cb_t (o)
{
}

cosched_aware_t &cosched_aware_t::operator= (const cosched_aware_t &o)
{
    dfu_match_cb_t::operator= (o);
    return *this;
}

cosched_aware_t::~cosched_aware_t ()
{
}

int cosched_aware_t::dom_finish_graph (subsystem_t subsystem,
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
    return 0;
}

int cosched_aware_t::dom_finish_slot (subsystem_t subsystem, scoring_api_t &dfu)
{
    std::vector<resource_type_t> types;
    dfu.resrc_types (subsystem, types);
    for (auto &type : types) {
        dfu.choose_accum_all (subsystem, type);
    }
    return 0;
}
/**
    IMPORTANT: Averaging Scores Across Nodes

    Overview:
    - Currently, scores are being set in two different locations:
      1. **Match Callback**: Scores are assigned to individual nodes.
      2. **Traverser**: Scores are assigned to parent nodes.

    Problem:
    - For GPU_MPS (a child node), we cannot directly set the score for its parent GPU node.
    - The parent GPU must first consider the state of its child nodes before setting its score.
    - The current approach involves:
      - Checking the GPU's state.
      - Determining how many of its child nodes are free.
      - Setting the GPU's score based on this information.

    Challenges:
    1. **Initializing Scores**:
       - When should the score for nodes be initialized?
       - When should the parent node rely on child node scores instead of setting its own?
    2. **Consistency**:
       - Ensuring a coherent relationship between child node scores and parent node scores during
traversal.
    3. Proposed logic:
       - For child nodes that are not yet traversed, the score should be set to `-1`.
       - For parent nodes whose scores have already been updated during traversal, skip reassigning
their scores.
**/

int cosched_aware_t::dom_finish_vtx (vtx_t u,
                                     subsystem_t subsystem,
                                     const std::vector<Flux::Jobspec::Resource> &resources,
                                     const resource_graph_t &g,
                                     scoring_api_t &dfu,
                                     traverser_match_kind_t sm)
{
    int64_t score = MATCH_MET;
    fold::less comp;
    /* Default value for worst-performing-class assumed as 9999. */
    int64_t perf_class = 9999;
    int64_t overall = score + g[u].id + perf_class;
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
    for (auto &resource : resources) {
        if (resource.type != Flux::resource_model::gpu_rt) {
            continue;
        }
        for (auto &c_resource : resource.with) {
            if (c_resource.type == Flux::resource_model::gpu_mps_rt) {
                unsigned int qc_1 = dfu.qualified_count (subsystem, c_resource.type);
                if (qc_1 == 2) {
                    overall = (score == MATCH_MET) ? (score + g[u].id + 1) : score;
                } else {
                    overall = (score == MATCH_MET) ? (score + g[u].id + 9999) : score;
                }
            }
        }
        if (g[u].type == Flux::resource_model::gpu_rt)
            dfu.set_overall_score (overall);
    }
    if (dfu.overall_score () == -1)
        dfu.set_overall_score (overall);
    if (sm == traverser_match_kind_t::SLOT_MATCH)
        dfu.set_overall_score (dfu.get_children_avearge_score ());

    decr ();
    return (score == MATCH_MET) ? 0 : -1;
}

int cosched_aware_t::dom_node_emit (vtx_t u,
                                    subsystem_t subsytem,
                                    const resource_graph_t &g,
                                    unsigned int needs)
{
    return 0;
}
}  // namespace resource_model
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
