/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef DFU_MATCH_CB_HPP
#define DFU_MATCH_CB_HPP

#include <set>
#include <map>
#include <vector>
#include <cstdint>
#include <iostream>
#include "resource/libjobspec/jobspec.hpp"
#include "resource/schema/resource_graph.hpp"
#include "resource/evaluators/scoring_api.hpp"
#include "resource/policies/base/matcher.hpp"
#include "resource/planner/c/planner.h"

namespace Flux {
namespace resource_model {

/*! Base DFU matcher class.
 *  Define the set of visitor methods that are called
 *  back by a DFU resource-graph traverser.
 */
class dfu_match_cb_t : public matcher_data_t, public matcher_util_api_t
{
public:
    dfu_match_cb_t ();
    dfu_match_cb_t (const std::string &name);
    dfu_match_cb_t (const dfu_match_cb_t &o);
    dfu_match_cb_t &operator= (const dfu_match_cb_t &o);
    virtual ~dfu_match_cb_t ();

    /*!
     *  Called back when all of the graph vertices and edges have been visited.
     *  Must be overridden by a derived class if this visit event should
     *  be programed.
     *
     *  \param subsystem subsystem_t object of the dominant subsystem.
     *  \param resources vector of resources to be matched.
     *  \param g         filtered resource graph.
     *  \param dfu       score interface object - See utilities/README.md
     *  \return          return 0 on success; otherwise -1.
     */
    virtual int dom_finish_graph (const subsystem_t &subsystem,
                                  const std::vector<
                                            Flux::Jobspec::Resource> &resources,
                                  const f_resource_graph_t &g,
                                  scoring_api_t &dfu);

    /*!
     * Called back on each postorder visit of a group of slot resources
     * (resources that can be contained within one or more slots) of the
     * dominant subsystem.
     */
    virtual int dom_finish_slot (const subsystem_t &subsystem,
                                 scoring_api_t &dfu);

    /*!
     *  Called back on each preorder visit of the dominant subsystem.
     *  Must be overridden by a derived class if this visit event should
     *  be programed.
     *
     *  \param u         descriptor of the visiting vertex.
     *  \param subsystem subsystem_t object of the dominant subsystem.
     *  \param resources vector of resources to be matched (resource section
     *                   of a jobspec).
     *  \param g         filtered resource graph.
     *
     *  \return          return 0 on success; otherwise -1.
     */
    virtual int dom_discover_vtx (vtx_t u,
                                  const subsystem_t &subsystem,
                                  const std::vector<
                                            Flux::Jobspec::Resource> &resources,
                                  const f_resource_graph_t &g);

    /*!
     *  Called back on each postorder visit of the dominant subsystem.
     *  Must be overridden by a derived class if this visit event should
     *  be programed.
     *  Should return a score calculated based on the subtree and up walks
     *  using the score API object (dfu). Any score above MATCH_MET
     *  is qualified to be a match.
     *
     *  \param u         descriptor of the visiting vertex
     *  \param subsystem subsystem_t object of the dominant subsystem
     *  \param resources vector of resources to be matched
     *  \param g         filtered resource graph
     *  \param dfu       score interface object -- See utilities/README.md
     *
     *  \return          return 0 on success; otherwise -1
     */
    virtual int dom_finish_vtx (vtx_t u,
                                const subsystem_t &subsystem,
                                const std::vector<
                                          Flux::Jobspec::Resource> &resources,
                                const f_resource_graph_t &g,
                                scoring_api_t &dfu);

    /*! Called back on each pre-up visit of an auxiliary subsystem.
     *  Must be overridden by a derived class if this visit event should
     *  be programed.
     *
     *  \param u         descriptor of the visiting vertex
     *  \param subsystem subsystem_t of the auxiliary subsystem being walked
     *  \param resources vector of resources to be matched
     *  \param g         filtered resource graph
     *
     *  \return          return 0 on success; otherwise -1
     */
    virtual int aux_discover_vtx (vtx_t u,
                                  const subsystem_t &subsystem,
                                  const std::vector<
                                            Flux::Jobspec::Resource> &resources,
                                  const f_resource_graph_t &g);

    /*
     *  Called back on each post-up visit of the auxiliary subsystem.
     *  Must be overridden by a derived class if this visit event should
     *  be programed. Should return a score calculated based on the subtree
     *  and up walks using the score API object (dfu).
     *  Any score above MATCH_MET is qualified to be a match.
     *
     *  \param u         descriptor of the visiting vertex
     *  \param subsystem subsystem_t object of an auxiliary subsystem
     *  \param resources vector of resources to be matched
     *  \param g         filtered resource graph object
     *  \param dfu       score interface object - -- See utilities/README.md
     *
     *  \return          return 0 on success; otherwise -1
     */
    virtual int aux_finish_vtx (vtx_t u,
                                const subsystem_t &subsystem,
                                const std::vector<
                                          Flux::Jobspec::Resource> &resources,
                                const f_resource_graph_t &g,
                                scoring_api_t &dfu);

    /*
     * Set a knob to limit graph traversal: i.g., stop traversing
     * when k instances of qualified matches are found for each requested
     * resource type.
     *
     *  \param k         num of qualified matches
     *
     *  \return          return 0 on success; otherwise -1
     */
    virtual int set_stop_on_k_matches (unsigned int k);

    /*
     * Return the knob to limit graph traversal: i.g., stop traversing
     * when k instances of qualified matches are found for each requested
     * resource type.
     */
    virtual int get_stop_on_k_matches () const;


    void incr ();
    void decr ();
    std::string level ();

private:
    int m_trav_level;
};

} // namespace resource_model
} // namespace Flux

#endif // DFU_MATCH_CB_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
