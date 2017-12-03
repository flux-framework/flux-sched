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

#ifndef MATCHER_DATA_HPP
#define MATCHER_DATA_HPP

#include <string>
#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <flux/jobspec.hpp>
#include "planner/planner.h"

namespace Flux {
namespace resource_model {

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
    matcher_data_t () : m_name ("anonymous") { }
    matcher_data_t (const std::string &name) : m_name (name) { }
    matcher_data_t (const matcher_data_t &o)
    {
       sdau_resource_types = o.sdau_resource_types;
       m_name = o.m_name;
       m_subsystems = o.m_subsystems;
       m_subsystems_map = o.m_subsystems_map;
    }
    matcher_data_t &operator=(const matcher_data_t &o)
    {
       sdau_resource_types = o.sdau_resource_types;
       m_name = o.m_name;
       m_subsystems = o.m_subsystems;
       m_subsystems_map = o.m_subsystems_map;
       return *this;
    }
    ~matcher_data_t ()
    {
        sdau_resource_types.clear ();
        m_subsystems.clear ();
        m_subsystems_map.clear ();
    }

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
    int add_subsystem (const subsystem_t s, const std::string tf = "*")
    {
        if (m_subsystems_map.find (s) == m_subsystems_map.end ()) {
            m_subsystems.push_back (s);
            m_subsystems_map[s].insert (tf);
            return 0;
        }
        return -1;
    }
    const std::string &matcher_name () const
    {
        return m_name;
    }
    void set_matcher_name (const std::string &name)
    {
        m_name = name;
    }
    const std::vector<subsystem_t> &subsystems () const
    {
        return m_subsystems;
    }

    /*
     * \return           return the dominant subsystem this matcher has
     *                   selected to use.
     */
    const subsystem_t &dom_subsystem () const
    {
        if (m_subsystems.begin () != m_subsystems.end ())
            return *(m_subsystems.begin());
        else
            return m_err_subsystem;
    }

    /*
     * \return           return the subsystem selector to be used for
     *                   graph filtering.
     */
    const multi_subsystemsS &subsystemsS () const
    {
        return m_subsystems_map;
    }

    unsigned int select_count (const Flux::Jobspec::Resource &resource,
                               unsigned int qc) const
    {
        if (resource.count.min > resource.count.max
            || resource.count.min > qc)
            return 0;

        unsigned int count = 0;
        unsigned int cur = resource.count.min;

        switch (resource.count.oper) {
        case '+':
            while (cur <= qc && cur <= resource.count.max) {
                count = cur;
                cur += resource.count.operand;
            }
            break;
        case '*':
            while (cur <= qc && cur <= resource.count.max) {
                count = cur;
                cur *= resource.count.operand;
            }
            break;
        case '^':
            if (resource.count.operand < 2)
                count = cur;
            else {
                while (cur <= qc && cur <= resource.count.max) {
                    unsigned int base = cur;
                    count = cur;
                    for (int i = 1;
                         i < resource.count.operand; i++)
                        cur *= base;
                }
            }
            break;
        default:
            break;
        }
        return count;
    }

    // resource types that will be used for scheduler driven aggregate updates
    std::map<subsystem_t, std::set<std::string>> sdau_resource_types;

private:
    std::string m_name;
    subsystem_t m_err_subsystem = "error";
    std::vector<subsystem_t> m_subsystems;
    multi_subsystemsS m_subsystems_map;
};

} // namespace resource_model
} // namespace Flux

#endif // MATCHER_DATA_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
