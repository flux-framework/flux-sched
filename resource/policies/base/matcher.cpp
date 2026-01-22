/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

#include <algorithm>
#include <limits>
#include "policies/base/matcher.hpp"

namespace Flux {
namespace resource_model {

const static resource_type_t ANY_RESOURCE_TYPE{"*"};

////////////////////////////////////////////////////////////////////////////////
// Matcher Data Method Definitions
////////////////////////////////////////////////////////////////////////////////

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

matcher_data_t &matcher_data_t::operator= (const matcher_data_t &o)
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

int matcher_data_t::add_subsystem (subsystem_t s, const std::string tf)
{
    if (m_subsystems_map[s].empty ()) {
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

subsystem_t matcher_data_t::dom_subsystem () const
{
    if (m_subsystems.begin () != m_subsystems.end ())
        return *(m_subsystems.begin ());
    else
        return m_err_subsystem;
}

const multi_subsystemsS &matcher_data_t::subsystemsS () const
{
    return m_subsystems_map;
}

////////////////////////////////////////////////////////////////////////////////
// Matcher Util Method Definitions
////////////////////////////////////////////////////////////////////////////////

unsigned int matcher_util_api_t::calc_count (const Flux::Jobspec::Resource &resource,
                                             unsigned int qc) const
{
    if (resource.count.min > resource.count.max || resource.count.min > qc)
        return 0;

    unsigned int range = 0;
    unsigned int count = 0;
    unsigned int cur = resource.count.min;

    switch (resource.count.oper) {
        case '+':
            count = (qc < resource.count.max) ? qc : resource.count.max;
            range = count - cur;
            count -= (range % resource.count.operand);
            break;
        case '*':
            if (resource.count.operand == 1)
                count = cur;
            else {
                while (cur <= qc && cur <= resource.count.max) {
                    count = cur;
                    cur *= resource.count.operand;
                }
            }
            break;
        case '^':
            if (resource.count.operand < 2 || cur == 1)
                count = cur;
            else {
                while (cur <= qc && cur <= resource.count.max) {
                    unsigned int base = cur;
                    count = cur;
                    for (int i = 1; i < resource.count.operand; i++)
                        cur *= base;
                }
            }
            break;
        default:
            break;
    }
    return count;
}

unsigned int matcher_util_api_t::calc_effective_max (const Flux::Jobspec::Resource &resource) const
{
    return calc_count (resource, std::numeric_limits<unsigned int>::max ());
}

int matcher_util_api_t::register_resource_pair (subsystem_t subsystem, std::string &r_pair)
{
    int rc = -1;
    size_t pos = 0;
    std::string h;
    std::string l;
    std::string split = ":";

    if ((pos = r_pair.find (split)) == std::string::npos)
        goto done;
    h = r_pair.substr (0, pos);
    h.erase (std::remove_if (h.begin (), h.end (), ::isspace), h.end ());
    if (h.empty ()) {
        errno = EINVAL;
        goto done;
    }
    if (h == "ALL")
        h = ANY_RESOURCE_TYPE.get ();
    l = r_pair.erase (0, pos + split.length ());
    l.erase (std::remove_if (l.begin (), l.end (), ::isspace), l.end ());
    if (l.empty ()) {
        errno = EINVAL;
        goto done;
    }
    set_pruning_type (subsystem, resource_type_t{h}, resource_type_t{l});
    rc = 0;

done:
    return rc;
}

int matcher_util_api_t::set_pruning_types_w_spec (subsystem_t subsystem, const std::string &spec)
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

void matcher_util_api_t::set_pruning_type (subsystem_t subsystem,
                                           resource_type_t anchor_type,
                                           resource_type_t prune_type)
{
    // Use operator[] to create an entry if subsystem key doesn't exist
    auto &s = m_pruning_types[subsystem];
    if (anchor_type == ANY_RESOURCE_TYPE) {
        // Check whether you have already installed prune_type.
        // If so, remove it as you want to install it against ANY_RESOURCE_TYPE.
        for (auto &kv : s)
            kv.second.erase (prune_type);
        // final container is "set" so it will only allow unique prune_types
        s[anchor_type].insert (prune_type);
    } else {
        if ((s.find (ANY_RESOURCE_TYPE) != s.end ())) {
            auto &prune_set = s[ANY_RESOURCE_TYPE];
            if (prune_set.find (prune_type) == prune_set.end ()) {
                // If prune_type does not exist against ANY_RESOURCE_TYPE
                // Install it against anchor_type, an individual resource type
                s[anchor_type].insert (prune_type);
            }  // orelse NOOP
        } else {
            s[anchor_type].insert (prune_type);
        }
    }
    m_total_set[subsystem].insert (prune_type);
}

bool matcher_util_api_t::is_my_pruning_type (subsystem_t subsystem,
                                             resource_type_t anchor_type,
                                             resource_type_t prune_type)
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

bool matcher_util_api_t::is_pruning_type (subsystem_t subsystem, resource_type_t prune_type)
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

bool matcher_util_api_t::get_my_pruning_types (subsystem_t subsystem,
                                               resource_type_t anchor_type,
                                               std::vector<resource_type_t> &out)
{
    bool rc = true;
    try {
        // Get the value of the subsystem, which is a map
        // of <string, set> type.
        auto &s = m_pruning_types.at (subsystem);
        if (s.find (anchor_type) != s.end ()) {
            // Get the value of the anchor map, which is a set
            auto &m = s.at (anchor_type);
            for (auto &k : m)
                out.push_back (k);
        }
        if (anchor_type != ANY_RESOURCE_TYPE && s.find (ANY_RESOURCE_TYPE) != s.end ()) {
            auto &m = s.at (ANY_RESOURCE_TYPE);
            for (auto &k : m) {
                if (anchor_type != k)
                    out.push_back (k);
            }
        }
    } catch (std::out_of_range &e) {
        rc = false;
    }
    return rc;
}

int matcher_util_api_t::add_exclusive_resource_type (resource_type_t type)
{
    auto ret = m_x_resource_types.insert (type);
    return ret.second ? 0 : -1;
}

const std::set<resource_type_t> &matcher_util_api_t::get_exclusive_resource_types () const
{
    return m_x_resource_types;
}

int matcher_util_api_t::reset_exclusive_resource_types (const std::set<resource_type_t> &x_types)
{
    m_x_resource_types.clear ();
    m_x_resource_types = x_types;
    return 0;
}

bool matcher_util_api_t::is_resource_type_exclusive (resource_type_t type)
{
    return m_x_resource_types.find (type) != m_x_resource_types.end ();
}

}  // namespace resource_model
}  // namespace Flux
/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
