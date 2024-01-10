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

/****************************************************************************
 *                                                                          *
 *                     Public Planner Multi Methods                         *
 *                                                                          *
 ****************************************************************************/

planner_multi::planner_multi () = default;

planner_multi::planner_multi (int64_t base_time, uint64_t duration,
                              const uint64_t *resource_totals,
                              const char **resource_types, size_t len)
{
    size_t i = 0;
    char *type = nullptr;
    planner_t *p = nullptr;

    m_iter.on_or_after = 0;
    m_iter.duration = 0;
    for (i = 0; i < len; ++i) {
        m_resource_totals.push_back (resource_totals[i]);
        if ( (type = strdup (resource_types[i])) == nullptr) {
            errno = ENOMEM;
            throw std::runtime_error ("ERROR in strdup\n");
        }
        m_resource_types.push_back (type);
        m_iter.counts.push_back (0);
        try {
           p = new planner_t (base_time, duration,
                              resource_totals[i],
                              resource_types[i]);
        } catch (std::bad_alloc &e) {
           errno = ENOMEM;
        }
        if (p == nullptr)
           throw std::runtime_error ("ERROR in planner_multi ctor\n");
        m_planners.push_back (p);
    }
    m_span_counter = 0;

}

planner_multi::planner_multi (const planner_multi &o)
{
    for (size_t i = 0; i < o.m_planners.size (); ++i) {
        planner_t *np = nullptr;
        if (o.m_planners[i]) {
            try {
                np = new planner_t (*(o.m_planners[i]->plan));
            } catch (std::bad_alloc &e) {
                errno = ENOMEM;
            }
            if (np == nullptr)
                throw std::runtime_error ("ERROR in planner_copy\n");
        } else {
            try {
                np = new planner_t ();
            } catch (std::bad_alloc &e) {
                errno = ENOMEM;
            }
            if (np == nullptr)
                throw std::runtime_error ("ERROR in planner_new_empty\n");
        }
        m_planners.push_back (np);
    }
    char *type = nullptr;
    for (size_t i = 0; i < o.m_resource_types.size (); ++i) {
        if ( (type = strdup (o.m_resource_types[i])) == nullptr) {
            errno = ENOMEM;
            throw std::runtime_error ("ERROR in strdup; ctor\n");
        }
        m_resource_types.push_back (type);
    }
    m_resource_totals = o.m_resource_totals;
    m_span_lookup = o.m_span_lookup;
    m_iter = o.m_iter;
    m_span_lookup_iter = o.m_span_lookup_iter;
    m_span_counter = o.m_span_counter;
}

planner_multi &planner_multi::operator= (const planner_multi &o)
{
    // Erase *this so the vectors are empty
    erase ();

    for (size_t i = 0; i < o.m_planners.size (); ++i) {
        planner_t *np = nullptr;
        if (o.m_planners[i]) {
            try {
                // Invoke copy constructor to avoid the assignment
                // overload erase () penalty.
                np = new planner_t (*(o.m_planners[i]->plan));
            } catch (std::bad_alloc &e) {
                errno = ENOMEM;
            }
            if (np == nullptr)
                throw std::runtime_error ("ERROR in planner_copy\n");
        } else {
            try {
                np = new planner_t ();
            } catch (std::bad_alloc &e) {
                errno = ENOMEM;
            }
            if (np == nullptr)
                throw std::runtime_error ("ERROR in planner_new_empty\n");
        }
        m_planners.push_back (np);
    }
    char *type = nullptr;
    for (size_t i = 0; i < o.m_resource_types.size (); ++i) {
        if ( (type = strdup (o.m_resource_types[i])) == nullptr) {
            errno = ENOMEM;
            throw std::runtime_error ("ERROR in strdup; assn overload\n");
        }
        m_resource_types.push_back (type);
    }
    m_resource_totals = o.m_resource_totals;
    m_span_lookup = o.m_span_lookup;
    m_iter = o.m_iter;
    m_span_lookup_iter = o.m_span_lookup_iter;
    m_span_counter = o.m_span_counter;

    return *this;
}

bool planner_multi::operator== (const planner_multi &o) const
{
    if (m_span_counter != o.m_span_counter)
        return false;
    if (m_resource_totals != o.m_resource_totals)
        return false;
    if (m_span_lookup != o.m_span_lookup)
        return false;
    if (m_iter.on_or_after != o.m_iter.on_or_after)
        return false;
    if (m_iter.duration != o.m_iter.duration)
        return false;
    if (m_iter.counts != o.m_iter.counts)
        return false;

    if (m_resource_types.size () != o.m_resource_types.size ())
        return false;
    for (size_t i = 0; i < m_resource_types.size (); ++i) {
        if (strcmp (m_resource_types[i], o.m_resource_types[i]) != 0)
            return false;
    }

    if (m_planners.size () != o.m_planners.size ())
        return false;
    for (size_t i = 0; i < m_planners.size (); ++i) {
        if (!(*(m_planners[i]->plan) == *(o.m_planners[i]->plan)))
            return false;
    }

    return true;
}

bool planner_multi::operator!= (const planner_multi &o) const
{
    return !operator == (o);
}

void planner_multi::erase ()
{
    if (!m_planners.empty ()) {
        size_t i = 0;
        for (i = 0; i < m_planners.size (); ++i) {
            if (m_planners[i]) {
                delete m_planners[i];
                m_planners[i] = nullptr;
            }
        }
    }
    if (!m_resource_types.empty ()) {
        size_t i = 0;
        for (i = 0; i < m_resource_types.size (); ++i) {
            if (m_resource_types[i]) {
                free ((void *)m_resource_types[i]);
                m_resource_types[i] = nullptr;
            }
        }
    }
    m_planners.clear ();
    m_resource_types.clear ();
    m_resource_totals.clear ();
    m_span_lookup.clear ();
}

planner_multi::~planner_multi ()
{
    erase ();
}

planner_t *planner_multi::get_planners_at (size_t i)
{
    return m_planners.at (i);
}

std::vector<planner_t *> &planner_multi::get_planners ()
{
    return m_planners;
}

size_t planner_multi::get_planners_size ()
{
    return m_planners.size ();
}

void planner_multi::resource_totals_push_back (const uint64_t resource_total)
{
    m_resource_totals.push_back (resource_total);
}

uint64_t planner_multi::get_resource_totals_at (size_t i)
{
    return m_resource_totals.at (i);
}

void planner_multi::resource_types_push_back (const char * resource_type)
{
    m_resource_types.push_back (resource_type);
}

const std::vector<const char *> planner_multi::get_resource_types ()
{
    return m_resource_types;
}

const char *planner_multi::get_resource_types_at (size_t i)
{
    return m_resource_types.at (i);
}

size_t planner_multi::get_resource_types_size ()
{
    return m_resource_types.size ();
}

struct request_multi &planner_multi::get_iter ()
{
    return m_iter;
}

std::map<uint64_t, std::vector<int64_t>> &planner_multi::get_span_lookup ()
{
    return m_span_lookup;
}

std::map<uint64_t, std::vector<int64_t>>::iterator
                                        &planner_multi::get_span_lookup_iter ()
{
    return m_span_lookup_iter;
}

void planner_multi::set_span_lookup_iter (std::map<uint64_t,
                                          std::vector<int64_t>>::iterator &it)
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

/****************************************************************************
 *                                                                          *
 *                     Public Planner_multi_t methods                       *
 *                                                                          *
 ****************************************************************************/

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

planner_multi_t::planner_multi_t (int64_t base_time, uint64_t duration,
                                  const uint64_t *resource_totals,
                                  const char **resource_types, size_t len)
{
    try {
        plan_multi = new planner_multi (base_time, duration, resource_totals,
                                        resource_types, len);
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
