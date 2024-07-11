/*****************************************************************************\
 * Copyright 2023 Lawrence Livermore National Security, LLC
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

#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <vector>
#include <map>

#include "planner_multi.hpp"

////////////////////////////////////////////////////////////////////////////////
// Public Planner Multi Methods
////////////////////////////////////////////////////////////////////////////////

planner_multi::planner_multi () = default;

planner_multi::planner_multi (int64_t base_time,
                              uint64_t duration,
                              const uint64_t *resource_totals,
                              const char **resource_types,
                              size_t len)
{
    size_t i = 0;
    std::string type;
    planner_t *p = nullptr;

    m_iter.on_or_after = 0;
    m_iter.duration = 0;
    for (i = 0; i < len; ++i) {
        try {
            type = std::string (resource_types[i]);
            p = new planner_t (base_time, duration, resource_totals[i], resource_types[i]);
        } catch (std::bad_alloc &e) {
            errno = ENOMEM;
            throw std::bad_alloc ();
        }
        m_iter.counts[type] = 0;
        m_types_totals_planners.push_back ({type, resource_totals[i], p});
    }
    m_span_counter = 0;
}

planner_multi::planner_multi (const planner_multi &o)
{
    for (auto &iter : o.m_types_totals_planners) {
        planner_t *np = nullptr;
        if (iter.planner) {
            try {
                np = new planner_t (*(iter.planner->plan));
            } catch (std::bad_alloc &e) {
                errno = ENOMEM;
            }
            // planner copy ctor can throw runtime_error, resulting in nullptr
            if (np == nullptr)
                throw std::runtime_error (
                    "ERROR in planner copy ctor"
                    " in planner_multi copy"
                    " constructor\n");
        } else {
            try {
                np = new planner_t ();
            } catch (std::bad_alloc &e) {
                errno = ENOMEM;
                throw std::bad_alloc ();
            }
        }
        m_types_totals_planners.push_back ({iter.resource_type, iter.resource_total, np});
    }
    m_iter = o.m_iter;
    m_span_lookup = o.m_span_lookup;
    m_span_lookup_iter = o.m_span_lookup_iter;
    m_span_counter = o.m_span_counter;
}

planner_multi &planner_multi::operator= (const planner_multi &o)
{
    // Erase *this so the vectors are empty
    erase ();

    for (const auto &iter : o.m_types_totals_planners) {
        planner_t *np = nullptr;
        if (iter.planner) {
            try {
                // Invoke copy constructor to avoid the assignment
                // operator erase () penalty.
                np = new planner_t (*(iter.planner->plan));
            } catch (std::bad_alloc &e) {
                errno = ENOMEM;
            }
            // planner copy ctor can throw runtime_error, resulting in nullptr
            if (np == nullptr)
                throw std::runtime_error (
                    "ERROR in planner copy ctor"
                    " in planner_multi assn"
                    " operator\n");
        } else {
            try {
                np = new planner_t ();
            } catch (std::bad_alloc &e) {
                errno = ENOMEM;
                throw std::bad_alloc ();
            }
        }
        m_types_totals_planners.push_back ({iter.resource_type, iter.resource_total, np});
    }
    m_iter = o.m_iter;
    m_span_lookup = o.m_span_lookup;
    m_span_lookup_iter = o.m_span_lookup_iter;
    m_span_counter = o.m_span_counter;

    return *this;
}

bool planner_multi::operator== (const planner_multi &o) const
{
    if (m_span_counter != o.m_span_counter)
        return false;
    if (m_span_lookup != o.m_span_lookup)
        return false;
    if (m_iter.on_or_after != o.m_iter.on_or_after)
        return false;
    if (m_iter.duration != o.m_iter.duration)
        return false;
    if (m_iter.counts != o.m_iter.counts)
        return false;

    if (m_types_totals_planners.size () != o.m_types_totals_planners.size ())
        return false;
    const auto &o_by_type = o.m_types_totals_planners.get<res_type> ();
    const auto &by_type = m_types_totals_planners.get<res_type> ();
    for (const auto &data : by_type) {
        const auto o_data = o_by_type.find (data.resource_type);
        if (o_data == o_by_type.end ())
            return false;
        if (data.resource_type != o_data->resource_type)
            return false;
        if (data.resource_total != o_data->resource_total)
            return false;
        if (*(data.planner->plan) != *(o_data->planner->plan))
            return false;
    }

    return true;
}

bool planner_multi::operator!= (const planner_multi &o) const
{
    return !operator== (o);
}

void planner_multi::erase ()
{
    if (!m_types_totals_planners.empty ()) {
        for (auto iter : m_types_totals_planners) {
            if (iter.planner) {
                delete iter.planner;
                iter.planner = nullptr;
            }
        }
    }
    m_types_totals_planners.clear ();
}

planner_multi::~planner_multi ()
{
    erase ();
}

void planner_multi::add_planner (int64_t base_time,
                                 uint64_t duration,
                                 const uint64_t resource_total,
                                 const char *resource_type,
                                 size_t i)
{
    std::string type;
    planner_t *p = nullptr;

    try {
        type = std::string (resource_type);
        p = new planner_t (base_time, duration, resource_total, resource_type);
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        throw std::bad_alloc ();
    }
    m_iter.counts[type] = 0;
    if (i > m_types_totals_planners.size ())
        m_types_totals_planners.push_back ({type, resource_total, p});
    else {
        auto it = m_types_totals_planners.begin () + i;
        m_types_totals_planners.insert (it, planner_multi_meta{type, resource_total, p});
    }
}

void planner_multi::delete_planners (const std::unordered_set<std::string> &rtypes)
{
    auto &by_res = m_types_totals_planners.get<res_type> ();
    for (auto iter = by_res.begin (); iter != by_res.end ();) {
        if (rtypes.find (iter->resource_type) == rtypes.end ()) {
            // need to remove from request_multi
            m_iter.counts.erase (iter->resource_type);
            // Trigger planner destructor
            delete iter->planner;
            iter = by_res.erase (iter);
        } else
            ++iter;
    }
}

planner_t *planner_multi::get_planner_at (size_t i) const
{
    return m_types_totals_planners.at (i).planner;
}

planner_t *planner_multi::get_planner_at (const char *type) const
{
    auto &by_res = m_types_totals_planners.get<res_type> ();
    return by_res.find (type)->planner;
}

void planner_multi::update_planner_index (const char *type, size_t i)
{
    auto by_res = m_types_totals_planners.get<res_type> ().find (type);
    auto new_idx = m_types_totals_planners.begin () + i;
    auto curr_idx = m_types_totals_planners.get<idx> ().iterator_to (*by_res);
    // noop if new_idx == curr_idx
    m_types_totals_planners.relocate (new_idx, curr_idx);
}

int planner_multi::update_planner_total (uint64_t total, size_t i)
{
    m_types_totals_planners.at (i).resource_total = total;
    return m_types_totals_planners.at (i).planner->plan->update_total (total);
}

bool planner_multi::planner_at (const char *type) const
{
    auto &by_res = m_types_totals_planners.get<res_type> ();
    auto result = by_res.find (type);
    if (result == by_res.end ())
        return false;
    else
        return true;
}

size_t planner_multi::get_planners_size () const
{
    return m_types_totals_planners.size ();
}

int64_t planner_multi::get_resource_total_at (size_t i) const
{
    return m_types_totals_planners.at (i).resource_total;
}

int64_t planner_multi::get_resource_total_at (const char *type) const
{
    auto &by_res = m_types_totals_planners.get<res_type> ();
    auto result = by_res.find (type);
    if (result == by_res.end ())
        return -1;
    else
        return result->resource_total;
}

const char *planner_multi::get_resource_type_at (size_t i) const
{
    return m_types_totals_planners.at (i).resource_type.c_str ();
}

size_t planner_multi::get_resource_type_idx (const char *type) const
{
    auto by_res = m_types_totals_planners.get<res_type> ().find (type);
    auto curr_idx = m_types_totals_planners.get<idx> ().iterator_to (*by_res);
    return curr_idx - m_types_totals_planners.begin ();
}

struct request_multi &planner_multi::get_iter ()
{
    return m_iter;
}

std::map<uint64_t, std::vector<int64_t>> &planner_multi::get_span_lookup ()
{
    return m_span_lookup;
}

std::map<uint64_t, std::vector<int64_t>>::iterator &planner_multi::get_span_lookup_iter ()
{
    return m_span_lookup_iter;
}

void planner_multi::set_span_lookup_iter (std::map<uint64_t, std::vector<int64_t>>::iterator &it)
{
    m_span_lookup_iter = it;
}

void planner_multi::incr_span_lookup_iter ()
{
    m_span_lookup_iter++;
}

uint64_t planner_multi::get_span_counter ()
{
    return m_span_counter;
}

void planner_multi::set_span_counter (uint64_t sc)
{
    m_span_counter = sc;
}

void planner_multi::incr_span_counter ()
{
    m_span_counter++;
}

////////////////////////////////////////////////////////////////////////////////
// Public Planner_multi_t methods
////////////////////////////////////////////////////////////////////////////////

planner_multi_t::planner_multi_t ()
{
    try {
        plan_multi = new planner_multi ();
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
    }
}

planner_multi_t::planner_multi_t (const planner_multi &o)
{
    try {
        plan_multi = new planner_multi (o);
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
    }
}

planner_multi_t::planner_multi_t (int64_t base_time,
                                  uint64_t duration,
                                  const uint64_t *resource_totals,
                                  const char **resource_types,
                                  size_t len)
{
    try {
        plan_multi = new planner_multi (base_time, duration, resource_totals, resource_types, len);
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
    }
}

planner_multi_t::~planner_multi_t ()
{
    delete plan_multi;
}

/*
 * vi: ts=4 sw=4 expandtab
 */
