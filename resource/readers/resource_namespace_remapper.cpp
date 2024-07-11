/*****************************************************************************\
 * Copyright 2020 Lawrence Livermore National Security, LLC
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

#include <vector>
#include <iostream>
#include <cerrno>
#include <cstdlib>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include "resource/readers/resource_namespace_remapper.hpp"

using namespace Flux::resource_model;

////////////////////////////////////////////////////////////////////////////////
// Public Resource Namespace Remapper API
////////////////////////////////////////////////////////////////////////////////

distinct_range_t::distinct_range_t (uint64_t point)
{
    m_low = point;
    m_high = point;
}

distinct_range_t::distinct_range_t (uint64_t lo, uint64_t hi)
{
    m_low = lo;
    m_high = hi;
    if (lo > hi)
        throw std::invalid_argument ("distinct_range_t: low > than high");
}

uint64_t distinct_range_t::get_low () const
{
    return m_low;
}

uint64_t distinct_range_t::get_high () const
{
    return m_high;
}

bool distinct_range_t::is_point () const
{
    return m_low == m_high;
}

bool distinct_range_t::operator< (const distinct_range_t &o) const
{
    // this class is used as key for std::map, which relies on the following
    // x and y are equivalent if !(x < y) && !(y < x)
    return m_high < o.m_low;
}

bool distinct_range_t::operator== (const distinct_range_t &o) const
{
    // x and y are equal if and only if both high and low
    // of the operands are equal
    return m_high == o.m_high && m_low == o.m_low;
}

bool distinct_range_t::operator!= (const distinct_range_t &o) const
{
    // x and y are not equal if either high or low is not equal
    return m_high != o.m_high || m_low != o.m_low;
}

int distinct_range_t::get_low_high (const std::string &exec_target_range,
                                    uint64_t &low,
                                    uint64_t &high)
{
    try {
        long int n;
        size_t ndash;
        std::string exec_target;
        std::istringstream istr{exec_target_range};
        std::vector<uint64_t> targets;

        if ((ndash = std::count (exec_target_range.begin (), exec_target_range.end (), '-')) > 1)
            goto inval;
        while (std::getline (istr, exec_target, '-')) {
            if ((n = std::stol (exec_target)) < 0)
                goto inval;
            targets.push_back (static_cast<uint64_t> (n));
        }
        low = high = targets[0];
        if (targets.size () == 2)
            high = targets[1];
        if (low > high)
            goto inval;
    } catch (std::invalid_argument &) {
        goto inval;
    } catch (std::out_of_range &) {
        errno = ERANGE;
        goto error;
    }
    return 0;
inval:
    errno = EINVAL;
error:
    return -1;
}

////////////////////////////////////////////////////////////////////////////////
// Public Resource Namespace Remapper API
////////////////////////////////////////////////////////////////////////////////

int resource_namespace_remapper_t::add (const uint64_t low,
                                        const uint64_t high,
                                        const std::string &name_type,
                                        uint64_t ref_id,
                                        uint64_t remapped_id)
{
    try {
        const distinct_range_t exec_target_range{low, high};
        auto m_remap_iter = m_remap.find (exec_target_range);

        if (m_remap_iter == m_remap.end ()) {
            m_remap.emplace (exec_target_range,
                             std::map<const std::string, std::map<uint64_t, uint64_t>> ());
        } else if (exec_target_range != m_remap_iter->first)
            goto inval;  // key must be exact

        if (m_remap[exec_target_range].find (name_type) == m_remap[exec_target_range].end ())
            m_remap[exec_target_range][name_type] = std::map<uint64_t, uint64_t> ();
        if (m_remap[exec_target_range][name_type].find (ref_id)
            != m_remap[exec_target_range][name_type].end ()) {
            errno = EEXIST;
            goto error;
        }
        m_remap[exec_target_range][name_type][ref_id] = remapped_id;
    } catch (std::bad_alloc &) {
        errno = ENOMEM;
        goto error;
    } catch (std::invalid_argument &) {
        goto inval;
    }
    return 0;
inval:
    errno = EINVAL;
error:
    return -1;
}

int resource_namespace_remapper_t::add (const std::string &exec_target_range,
                                        const std::string &name_type,
                                        uint64_t ref_id,
                                        uint64_t remapped_id)
{
    try {
        uint64_t low, high;
        if (distinct_range_t::get_low_high (exec_target_range, low, high) < 0)
            goto error;
        return add (low, high, name_type, ref_id, remapped_id);
    } catch (std::bad_alloc &) {
        errno = ENOMEM;
        goto error;
    }
    return 0;
error:
    return -1;
}

int resource_namespace_remapper_t::add_exec_target_range (
    const std::string &exec_target_range,
    const distinct_range_t &remapped_exec_target_range)
{
    try {
        uint64_t low, high, r_low, r_high, i, j;
        if (distinct_range_t::get_low_high (exec_target_range, low, high) < 0)
            goto error;
        r_low = remapped_exec_target_range.get_low ();
        r_high = remapped_exec_target_range.get_high ();
        if ((high - low) != (r_high - r_low))
            goto inval;
        for (i = low, j = r_low; i <= high && j <= r_high; i++, j++) {
            if (add (exec_target_range, "exec-target", i, j) < 0)
                goto error;
        }
    } catch (std::bad_alloc &) {
        errno = ENOMEM;
        goto error;
    }
    return 0;
inval:
    errno = EINVAL;
error:
    return -1;
}

int resource_namespace_remapper_t::query (const uint64_t exec_target,
                                          const std::string &name_type,
                                          uint64_t ref_id,
                                          uint64_t &remapped_id_out) const
{
    try {
        remapped_id_out = m_remap.at (distinct_range_t{exec_target}).at (name_type).at (ref_id);
        return 0;
    } catch (std::out_of_range &) {
        errno = ENOENT;
        return -1;
    }
}

int resource_namespace_remapper_t::query_exec_target (const uint64_t exec_target,
                                                      uint64_t &remapped_exec_target) const
{
    try {
        remapped_exec_target =
            m_remap.at (distinct_range_t{exec_target}).at ("exec-target").at (exec_target);
        return 0;
    } catch (std::out_of_range &) {
        errno = ENOENT;
        return -1;
    }
}

bool resource_namespace_remapper_t::is_remapped () const
{
    return !m_remap.empty ();
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
