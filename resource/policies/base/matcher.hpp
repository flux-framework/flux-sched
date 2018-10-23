/*****************************************************************************\
 *  Copyright (c) 2014 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
 *  LLNL-CODE-658032 All rights reserved.
 *
 *  This file is part of the Flux resource manager framework.
 *  For details, see https://github.com/flux-framework.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license, or (at your option)
 *  any later version.
 *
 *  Flux is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *  See also:  http://www.gnu.org/licenses/
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
#include "resource/planner/planner.h"

namespace Flux {
namespace resource_model {

const std::string ANY_RESOURCE_TYPE = "*";

enum match_score_t { MATCH_UNMET = 0, MATCH_MET = 1 };

enum class match_op_t { MATCH_ALLOCATE, MATCH_ALLOCATE_ORELSE_RESERVE };

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

    void set_pruning_type (const std::string &subsystem,
                           const std::string &anchor_type,
                           const std::string &prune_type);
    bool is_my_pruning_type (const std::string &subsystem,
                             const std::string &anchor_type,
                             const std::string &prune_type);
    bool is_pruning_type (const std::string &subsystem,
                          const std::string &prune_type);

private:
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
