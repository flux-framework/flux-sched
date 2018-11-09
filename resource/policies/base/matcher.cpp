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

#include <algorithm>

#include "policies/base/matcher.hpp"

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

int matcher_util_api_t::register_resource_pair (const std::string &subsystem,
                                                std::string &r_pair)
{
    int rc = -1;
    size_t pos = 0;
    std::string h;
    std::string l;
    std::string split = ":";

    if (((pos = r_pair.find (split)) == std::string::npos))
        goto done;
    h = r_pair.substr (0, pos);
    h.erase (std::remove_if (h.begin (), h.end (), ::isspace), h.end ());
    if (h.empty ()) {
        errno = EINVAL;
        goto done;
    }
    if (h == "ALL")
        h = ANY_RESOURCE_TYPE;
    l = r_pair.erase (0, pos + split.length ());
    l.erase (std::remove_if (l.begin (), l.end (), ::isspace), l.end ());
    if (l.empty ()) {
        errno = EINVAL;
        goto done;
    }
    set_pruning_type (subsystem, h, l);
    rc = 0;

done:
    return rc;
}

int matcher_util_api_t::set_pruning_types_w_spec (const std::string &subsystem,
                                                  const std::string &spec)
{
    int rc = -1;
    size_t pos = 0;
    std::string spec_copy = spec;
    std::string sep = ",";

    try {
        while ((pos = spec_copy.find (sep)) != std::string::npos) {
            std::string r_pair = spec_copy.substr (0, pos);
            if (register_resource_pair (subsystem, r_pair) < 0)
                goto done;
            spec_copy.erase (0, pos + sep.length ());
        }
        if (register_resource_pair (subsystem, spec_copy) < 0)
            goto done;
        rc = 0;
    } catch (std::out_of_range &e) {
        errno = EINVAL;
        rc = -1;
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        rc = -1;
    }

done:
    return rc;
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
    bool rc = false;
    try {
        auto &s = m_pruning_types.at (subsystem);
        if (s.find (anchor_type) != s.end ()) {
            auto &m = s.at (anchor_type);
            rc = (m.find (prune_type) != m.end ());
        } else if (s.find (ANY_RESOURCE_TYPE) != s.end ()) {
            auto &m = s.at (ANY_RESOURCE_TYPE);
            rc = (m.find (prune_type) != m.end ());
        }
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
