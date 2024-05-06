/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef DFU_MATCH_LOCALITY_HPP
#define DFU_MATCH_LOCALITY_HPP

#include <iostream>
#include <vector>
#include <numeric>
#include <map>
#include <boost/icl/interval.hpp>
#include <boost/icl/interval_set.hpp>
#include "resource/policies/base/dfu_match_cb.hpp"

namespace Flux {
namespace resource_model {

/*! Locality-aware policy: select resources of each type
 *  where you have more qualified.
 */
struct greater_interval_first_t : public dfu_match_cb_t
{
    greater_interval_first_t ();
    greater_interval_first_t (const std::string &name);
    greater_interval_first_t (const greater_interval_first_t &o);
    greater_interval_first_t &operator= (const greater_interval_first_t &o);
    ~greater_interval_first_t ();

    int dom_finish_graph (const subsystem_t &subsystem,
                          const std::vector<Flux::Jobspec::Resource> &resources,
                          const f_resource_graph_t &g, scoring_api_t &dfu);

    int dom_finish_vtx (vtx_t u, const subsystem_t &subsystem,
                        const std::vector<Flux::Jobspec::Resource> &resources,
                        const f_resource_graph_t &g, scoring_api_t &dfu);

    int dom_finish_slot (const subsystem_t &subsystem, scoring_api_t &dfu);
};

}
}

#endif //DFU_MATCH_LOCALITY_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
