/*****************************************************************************\
 * Copyright 2023 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef PLANNER_HPP
#define PLANNER_HPP

#include <memory>
#include "planner_internal_tree.hpp"

struct request_t {
    int64_t on_or_after;
    uint64_t duration;
    int64_t count;
};

/*! Node in a span interval tree to enable fast retrieval of intercepting spans.
 */
struct span_t {
    bool operator== (const span_t &o) const;
    bool operator!= (const span_t &o) const;

    int64_t start;              /* start time of the span */
    int64_t last;               /* end time of the span */
    int64_t span_id;            /* unique span id */
    int64_t planned;            /* required resource quantity */
    int in_system;              /* 1 when inserted into the system */
    scheduled_point_t *start_p; /* scheduled point object at start */
    scheduled_point_t *last_p;  /* scheduled point object at last */
};

/*! Planner class
 */
class planner {
   public:
    planner ();
    planner (const int64_t base_time,
             const uint64_t duration,
             const uint64_t resource_totals,
             const char *in_resource_type);
    planner (const planner &o);
    planner &operator= (const planner &o);
    bool operator== (const planner &o) const;
    bool operator!= (const planner &o) const;
    ~planner ();
    // Public class utilities
    int erase ();
    int reinitialize (int64_t base_time, uint64_t duration);
    int restore_track_points ();
    int update_total (uint64_t resource_total);

    // Resources and duration
    int64_t get_total_resources () const;
    const std::string &get_resource_type () const;
    int64_t get_plan_start () const;
    int64_t get_plan_end () const;
    // RBTree functions
    int mt_tree_insert (scheduled_point_t *point);
    int mt_tree_remove (scheduled_point_t *point);
    int sp_tree_insert (scheduled_point_t *point);
    int sp_tree_remove (scheduled_point_t *point);
    void destroy_sp_tree ();
    scheduled_point_t *sp_tree_search (int64_t at);
    scheduled_point_t *sp_tree_get_state (int64_t at);
    scheduled_point_t *sp_tree_next (scheduled_point_t *point) const;
    scheduled_point_t *mt_tree_get_mintime (int64_t request) const;
    // Span lookup functions
    void clear_span_lookup ();
    void span_lookup_erase (std::map<int64_t, std::shared_ptr<span_t>>::iterator &it);
    const std::map<int64_t, std::shared_ptr<span_t>> &get_span_lookup_const () const;
    std::map<int64_t, std::shared_ptr<span_t>> &get_span_lookup ();
    size_t span_lookup_get_size () const;
    void span_lookup_insert (int64_t span_id, std::shared_ptr<span_t> span);
    const std::map<int64_t, std::shared_ptr<span_t>>::iterator get_span_lookup_iter () const;
    void set_span_lookup_iter (std::map<int64_t, std::shared_ptr<span_t>>::iterator &it);
    void incr_span_lookup_iter ();
    // Avail_time functions
    std::map<int64_t, scheduled_point_t *> &get_avail_time_iter ();
    const std::map<int64_t, scheduled_point_t *> &get_avail_time_iter_const () const;
    void clear_avail_time_iter ();
    void set_avail_time_iter_set (int atime_iter_set);
    const int get_avail_time_iter_set () const;
    // Request functions
    request_t &get_current_request ();
    const request_t &get_current_request_const () const;
    // Span counter functions
    void incr_span_counter ();
    const uint64_t get_span_counter () const;

   private:
    int64_t m_total_resources = 0;
    std::string m_resource_type = "";
    int64_t m_plan_start = 0;                   /* base time of the planner */
    int64_t m_plan_end = 0;                     /* end time of the planner */
    scheduled_point_tree_t m_sched_point_tree;  /* scheduled point rb tree */
    mintime_resource_tree_t m_mt_resource_tree; /* min-time resource rb tree */
    scheduled_point_t *m_p0 = nullptr;          /* system's scheduled point at base time */
    std::map<int64_t, std::shared_ptr<span_t>> m_span_lookup; /* span lookup */
    std::map<int64_t, std::shared_ptr<span_t>>::iterator m_span_lookup_iter;
    std::map<int64_t, scheduled_point_t *> m_avail_time_iter; /* MT node track */
    int m_avail_time_iter_set = 0;                            /* iterator set flag */
    request_t m_current_request; /* the req copy for avail time iteration */
    uint64_t m_span_counter = 0; /* current span counter */
    // Private class utilities
    int copy_trees (const planner &o);
    int copy_maps (const planner &o);
    bool span_lookups_equal (const planner &o) const;
    bool avail_time_iters_equal (const planner &o) const;
    bool trees_equal (const planner &o) const;
};

struct planner_t {
    planner_t ();
    planner_t (const planner &o);
    planner_t (const int64_t base_time,
               const uint64_t duration,
               const uint64_t resource_totals,
               const char *in_resource_type);
    ~planner_t ();

    planner *plan = nullptr;
};

#endif /* PLANNER_HPP */

/*
 * vi: ts=4 sw=4 expandtab
 */
