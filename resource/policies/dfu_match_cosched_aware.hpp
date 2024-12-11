/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef DFU_MATCH_COSCHED_AWARE_HPP
#define DFU_MATCH_COSCHED_AWARE_HPP

#include <iostream>
#include <vector>
#include <numeric>
#include <map>
#include "resource/policies/base/dfu_match_cb.hpp"

namespace Flux {
namespace resource_model {

/*! coschediation-aware policy: allocate resources based on
 * similar performance class.
 */
struct cosched_aware_t : public dfu_match_cb_t {
    cosched_aware_t ();
    cosched_aware_t (const std::string &name);
    cosched_aware_t (const cosched_aware_t &o);
    cosched_aware_t &operator= (const cosched_aware_t &o);
    ~cosched_aware_t ();

    int dom_finish_graph (subsystem_t subsystem,
                          const std::vector<Flux::Jobspec::Resource> &resources,
                          const resource_graph_t &g,
                          scoring_api_t &dfu);
    int dom_finish_vtx (vtx_t u,
                        subsystem_t subsystem,
                        const std::vector<Flux::Jobspec::Resource> &resources,
                        const resource_graph_t &g,
                        scoring_api_t &dfu,
                        traverser_match_kind_t sm);

    int dom_finish_slot (subsystem_t subsystem, scoring_api_t &dfu);

    int dom_node_emit (vtx_t u, subsystem_t subsytem, const resource_graph_t &g,unsigned int needs);
};
}  // namespace resource_model
}  // namespace Flux

#endif  // DFU_MATCH_cosched_AWARE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
