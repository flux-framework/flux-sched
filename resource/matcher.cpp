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

#include "matcher.hpp"

namespace Flux {
namespace resource_model {

/****************************************************************************
 *                                                                          *
 *                    Matcher Data Method Definitions                       *
 *                                                                          *
 ****************************************************************************/

matcher_data_t::matcher_data_t () : m_name ("anonymous")
{

}

matcher_data_t::matcher_data_t (const std::string &name) : m_name (name)
{

}

matcher_data_t::matcher_data_t (const matcher_data_t &o)
{
    m_name = o.m_name;
    m_subsystems = o.m_subsystems;
    m_subsystems_map = o.m_subsystems_map;
}

matcher_data_t &matcher_data_t::operator=(const matcher_data_t &o)
{
    m_name = o.m_name;
    m_subsystems = o.m_subsystems;
    m_subsystems_map = o.m_subsystems_map;
    return *this;
}

matcher_data_t::~matcher_data_t ()
{
    m_subsystems.clear ();
    m_subsystems_map.clear ();
}

int matcher_data_t::add_subsystem (const subsystem_t s, const std::string tf)
{
    if (m_subsystems_map.find (s) == m_subsystems_map.end ()) {
        m_subsystems.push_back (s);
        m_subsystems_map[s].insert (tf);
        return 0;
    }
    return -1;
}

const std::string &matcher_data_t::matcher_name () const
{
    return m_name;
}

void matcher_data_t::set_matcher_name (const std::string &name)
{
    m_name = name;
}

const std::vector<subsystem_t> &matcher_data_t::subsystems () const
{
    return m_subsystems;
}

const subsystem_t &matcher_data_t::dom_subsystem () const
{
    if (m_subsystems.begin () != m_subsystems.end ())
        return *(m_subsystems.begin());
    else
        return m_err_subsystem;
}

const multi_subsystemsS &matcher_data_t::subsystemsS () const
{
    return m_subsystems_map;
}


/****************************************************************************
 *                                                                          *
 *                      Matcher Util Method Definitions                     *
 *                                                                          *
 ****************************************************************************/

unsigned int matcher_util_api_t::calc_count (
    const Flux::Jobspec::Resource &resource,
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

void matcher_util_api_t::set_pruning_type (const std::string &subsystem,
                                           const std::string &anchor_type,
                                           const std::string &prune_type)
{
    m_pruning_types[subsystem][anchor_type].insert (prune_type);
    m_total_set[subsystem].insert (prune_type);
}

bool matcher_util_api_t::is_my_pruning_type (const std::string &subsystem,
                                             const std::string &anchor_type,
                                             const std::string &prune_type)
{
    bool rc = true;
    try {
        auto &s = m_pruning_types.at (subsystem).at (anchor_type);
        rc = (s.find (prune_type) != s.end ());
    } catch (std::out_of_range &e) {
        rc = false;
    }
    return rc;
}

bool matcher_util_api_t::is_pruning_type (const std::string &subsystem,
                                          const std::string &prune_type)
{
    bool rc = true;
    try {
        auto &s = m_total_set.at (subsystem);
        rc = (s.find (prune_type) != s.end ());
    } catch (std::out_of_range &e) {
        rc = false;
    }
    return rc;
}

} // resource_model
} // Flux
/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
