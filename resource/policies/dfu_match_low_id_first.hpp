/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef DFU_MATCH_LOW_ID_FIRST_HPP
#define DFU_MATCH_LOW_ID_FIRST_HPP

#include <iostream>
#include <vector>
#include <numeric>
#include <map>
#include "resource/policies/base/dfu_match_cb.hpp"

namespace Flux {
namespace resource_model {

/*! Low ID first policy: select resources of each type
 *  with lower numeric IDs.
 */
class low_first_t : public dfu_match_cb_t
{
public:
    low_first_t ();
    low_first_t (const std::string &name);
    low_first_t (const low_first_t &o);
    low_first_t &operator= (const low_first_t &o);
    ~low_first_t ();

    int dom_finish_graph (const subsystem_t &subsystem,
                          const std::vector<Flux::Jobspec::Resource> &resources,
                          const f_resource_graph_t &g, scoring_api_t &dfu);
    int dom_finish_vtx (vtx_t u, const subsystem_t &subsystem,
                        const std::vector<Flux::Jobspec::Resource> &resources,
                        const f_resource_graph_t &g, scoring_api_t &dfu);

    int dom_finish_slot (const subsystem_t &subsystem, scoring_api_t &dfu);

    /*
     * Set a knob to limit graph traversal: i.g., stop traversing
     * when k instances of qualifed matches are found
     * for each requested resource type.
     *
     *  \param k         num of qualified matches
     *
     *  \return          return 0 on success; -1 if k is great than 1
     *                   (i.e., only first match is currently supported).
     */
    virtual int set_stop_on_k_matches (unsigned int k);

    /*
     * Return the knob to limit graph traversal: i.g., stop traversing
     * when k instances of qualifed matches are found
     * for each requested resource type.
     */
    virtual int get_stop_on_k_matches () const;

private:
    unsigned int m_stop_on_k_matches = 0;
};

} // resource_model
} // Flux

#endif // DFU_MATCH_LOW_ID_FIRST_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
