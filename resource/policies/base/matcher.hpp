/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef MATCHER_HPP
#define MATCHER_HPP

#include <string>
#include <iostream>
#include <vector>
#include <set>
#include <map>
#include "resource/libjobspec/jobspec.hpp"
#include "resource/schema/data_std.hpp"
#include "resource/planner/c/planner.h"

namespace Flux {
namespace resource_model {

const std::string ANY_RESOURCE_TYPE = "*";

enum match_score_t { MATCH_UNMET = 0, MATCH_MET = 1 };

enum class match_op_t { MATCH_ALLOCATE,
                        MATCH_ALLOCATE_W_SATISFIABILITY,
                        MATCH_ALLOCATE_ORELSE_RESERVE,
                        MATCH_SATISFIABILITY };

/*! Base matcher data class.
 *  Provide idioms to specify the target subsystems and
 *  resource relationship types which then allow for filtering the graph
 *  data store for the match callback object.
 */
class matcher_data_t
{
public:
    matcher_data_t ();
    matcher_data_t (const std::string &name);
    matcher_data_t (const matcher_data_t &o);
    matcher_data_t &operator=(const matcher_data_t &o);
    ~matcher_data_t ();

    /*! Add a subsystem and the relationship type that this resource base
     *  matcher will use. Vertices and edges of the resource graph are
     *  filtered based on this information. Each vertex and edge that
     *  is a part of this subsystem and relationship type will be selected.
     *
     *  This method must be called at least once to set the dominant
     *  subsystem to use. This method can be called multiple times with
     *  a distinct subsystem, each additional subsystem becomes an auxiliary
     *  subsystem.
     *
     *  \param subsystem subsystem to select
     *  \param tf        edge (or relation type) to select.
     *                   pass * to select all types
     *  \return          0 on success; -1 on error
     */
    int add_subsystem (const subsystem_t s, const std::string tf = "*");

    const std::string &matcher_name () const;
    void set_matcher_name (const std::string &name);
    const std::vector<subsystem_t> &subsystems () const;

    /*
     * \return           return the dominant subsystem this matcher has
     *                   selected to use.
     */
    const subsystem_t &dom_subsystem () const;

    /*
     * \return           return the subsystem selector to be used for
     *                   graph filtering.
     */
    const multi_subsystemsS &subsystemsS () const;

private:
    std::string m_name;
    subsystem_t m_err_subsystem = "error";
    std::vector<subsystem_t> m_subsystems;
    multi_subsystemsS m_subsystems_map;
};


/*! Base utility method for matchers.
 */
class matcher_util_api_t
{
public:

    /*! Calculate the count that should be allocated, which is a function
     *  of the number of qualifed available resources, minimum/maximum/operator
     *  requirement of the jobspec.
     *
     *  \param resource  resource section of the jobspec.
     *  \param qual_cnt  available qualified resources.
     *  \return          0 when not available; count otherwise.
     */
    unsigned int calc_count (const Flux::Jobspec::Resource &resource,
                             unsigned int qual_cnt) const;

    /*! Calculate the effective max count that should be allocated.
     *
     *  \param resource  resource section of the jobspec.
     *  \return          Effective max count; 0 if an error is encountered.
     */
    unsigned int calc_effective_max (
                     const Flux::Jobspec::Resource &resource) const;


    /*! Set prune filters based on spec. The spec should comply with
     *  the following format:
     *  <HL-resource1:LL-resource1[,HL-resource2:LL-resource2...]...]>
     *      Install a planner-based filter at each High-Level (HL) resource
     *      vertex which tracks the state of the Low-Level (LL) resources
     *      in aggregate, residing under its subtree. If a jobspec requests
     *      1 node with 4 cores, and the visiting compute-node vertex has
     *      only a total of 2 available cores in aggregate at its
     *      subtree, this filter allows the traverser to prune a further descent
     *      to accelerate the search.
     *      Use the ALL keyword for HL-resource if you want LL-resource to be
     *      tracked at all of the HL-resource vertices. Examples:
     *      rack:node,node:core
     *      ALL:core,cluster:node,rack:node
     *
     *  \param subsystem subsystem
     *  \param spec      prune filter specification string
     */
    int set_pruning_types_w_spec (const std::string &subsystem,
                                  const std::string &spec);

    void set_pruning_type (const std::string &subsystem,
                           const std::string &anchor_type,
                           const std::string &prune_type);

    bool is_my_pruning_type (const std::string &subsystem,
                             const std::string &anchor_type,
                             const std::string &prune_type);

    bool is_pruning_type (const std::string &subsystem,
                          const std::string &prune_type);


    bool get_my_pruning_types (const std::string &subsystem,
                               const std::string &anchor_type,
                               std::vector<std::string> &out_prune_types);

    int add_exclusive_resource_type (const std::string &type);

    bool is_resource_type_exclusive (const std::string &type);

    const std::set<std::string> &get_exclusive_resource_types () const;

    int reset_exclusive_resource_types (const std::set<std::string> &x_types);

private:

    int register_resource_pair (const std::string &subsystem,
                                std::string &r_pair);

    std::set<std::string> m_x_resource_types;

    // resource types that will be used for scheduler driven aggregate updates
    // Examples:
    // m_pruning_type["containment"]["rack"] -> {"node"}
    //     in the containment subsystem at the rack level, please
    //     maintain an aggregate on the available nodes under it.
    // m_pruing_type["containment"][ANY_RESOURCE_TYPE] -> {"core"}
    //     in the containment subsystem at any level, please
    //     maintain an aggregate on the available cores under it.
    std::map<subsystem_t,
             std::map<std::string, std::set<std::string>>> m_pruning_types;
    std::map<subsystem_t, std::set<std::string>> m_total_set;
};

} // namespace resource_model
} // namespace Flux

#endif // MATCHER_DATA_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
