/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef DFU_MATCH_VAR_AWARE_HPP
#define DFU_MATCH_VAR_AWARE_HPP

#include <iostream>
#include <vector>
#include <numeric>
#include <map>
#include "resource/policies/base/dfu_match_cb.hpp"

namespace Flux {
namespace resource_model {

/*! Variation-aware policy: allocate resources based on
 * similar performance class.
 */
struct var_aware_t : public dfu_match_cb_t
{
    var_aware_t ();
    var_aware_t (const std::string &name);
    var_aware_t (const var_aware_t &o);
    var_aware_t &operator= (const var_aware_t &o);
    ~var_aware_t ();

    int dom_finish_graph (const subsystem_t &subsystem,
                          const std::vector<Flux::Jobspec::Resource> &resources,
                          const f_resource_graph_t &g, scoring_api_t &dfu);
    int dom_finish_vtx (vtx_t u, const subsystem_t &subsystem,
                        const std::vector<Flux::Jobspec::Resource> &resources,
                        const f_resource_graph_t &g, scoring_api_t &dfu);

    int dom_finish_slot (const subsystem_t &subsystem, scoring_api_t &dfu);
};

} // resource_model
} // Flux

#endif // DFU_MATCH_VAR_AWARE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
