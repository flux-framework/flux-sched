/*****************************************************************************\
 * Copyright 2023 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef PLANNER_MULTI_HPP
#define PLANNER_MULTI_HPP

#include "planner.hpp"

struct request_multi {
    int64_t on_or_after = 0;
    uint64_t duration = 0;
    std::vector<int64_t> counts;
};

class planner_multi {
public:
    planner_multi ();
    planner_multi (int64_t base_time, uint64_t duration,
                   const uint64_t *resource_totals,
                   const char **resource_types, size_t len);
    planner_multi (const planner_multi &o);
    planner_multi &operator= (const planner_multi &o);
    bool operator== (const planner_multi &o) const;
    void erase ();
    ~planner_multi ();

    // Public getters and setters
    planner_t *get_planners_at (size_t i);
    std::vector<planner_t *> &get_planners ();
    size_t get_planners_size ();
    void resource_totals_push_back (const uint64_t resource_total);
    uint64_t get_resource_totals_at (size_t i);
    void resource_types_push_back (const char * resource_type);
    const std::vector<const char *> get_resource_types ();
    const char *get_resource_types_at (size_t i);
    size_t get_resource_types_size ();
    struct request_multi &get_iter ();
    // Span lookup functions
    std::map<uint64_t, std::vector<int64_t>> &get_span_lookup ();
    std::map<uint64_t, std::vector<int64_t>>::iterator
                                &get_span_lookup_iter ();
    void set_span_lookup_iter (std::map<uint64_t,
                               std::vector<int64_t>>::iterator &it);
    void incr_span_lookup_iter ();
    // Get and set span_counter
    uint64_t get_span_counter ();
    void set_span_counter (uint64_t sc);
    void incr_span_counter ();

    // These need to be public to pass references, otherwise
    // need to pass tmp variables by reference.
    std::vector<const char *> m_resource_types;
    std::vector<uint64_t> m_resource_totals;
    std::vector<planner_t *> m_planners;

private:
    struct request_multi m_iter;
    std::map<uint64_t, std::vector<int64_t>> m_span_lookup;
    std::map<uint64_t, std::vector<int64_t>>::iterator m_span_lookup_iter;
    uint64_t m_span_counter = 0;
};

struct planner_multi_t {
    planner_multi_t ();
    planner_multi_t (const planner_multi &o);
    planner_multi_t (int64_t base_time, uint64_t duration,
                     const uint64_t *resource_totals,
                     const char **resource_types, size_t len);
    ~planner_multi_t ();

    planner_multi *plan_multi = nullptr;
};

#endif /* PLANNER_MULTI_HPP */

/*
 * vi: ts=4 sw=4 expandtab
 */
