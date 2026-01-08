/*****************************************************************************\
 * Copyright 2023 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#include <limits>
#include <map>
#include <list>
#include <string>

#include "resource/planner/c++/planner.hpp"

////////////////////////////////////////////////////////////////////////////////
// Scheduled Point and Resource Update APIs
////////////////////////////////////////////////////////////////////////////////

static int track_points (std::map<int64_t, scheduled_point_t *> &tracker, scheduled_point_t *point)
{
    // caller will rely on the fact that rc == -1 when key already exists.
    // don't need to register free */
    auto res = tracker.insert (std::pair<int64_t, scheduled_point_t *> (point->at, point));
    return res.second ? 0 : -1;
}

static void restore_track_points (planner_t *ctx)
{
    ctx->plan->restore_track_points ();
}

static void update_mintime_resource_tree (planner_t *ctx, std::list<scheduled_point_t *> &list)
{
    for (auto &point : list) {
        if (point->in_mt_resource_tree)
            ctx->plan->mt_tree_remove (point);
        if (point->ref_count && !(point->in_mt_resource_tree))
            ctx->plan->mt_tree_insert (point);
    }
}

static void copy_req (request_t &dest,
                      int64_t on_or_after,
                      uint64_t duration,
                      uint64_t resource_count)
{
    dest.on_or_after = on_or_after;
    dest.duration = duration;
    dest.count = static_cast<int64_t> (resource_count);
}

static scheduled_point_t *get_or_new_point (planner_t *ctx, int64_t at)
{
    scheduled_point_t *point = nullptr;
    try {
        if (!(point = ctx->plan->sp_tree_search (at))) {
            scheduled_point_t *state = ctx->plan->sp_tree_get_state (at);
            point = new scheduled_point_t ();
            point->at = at;
            point->in_mt_resource_tree = 0;
            point->new_point = 1;
            point->ref_count = 0;
            point->scheduled = state->scheduled;
            point->remaining = state->remaining;
            ctx->plan->sp_tree_insert (point);
            ctx->plan->mt_tree_insert (point);
        }
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
    }
    return point;
}

static void fetch_overlap_points (planner_t *ctx,
                                  int64_t at,
                                  uint64_t duration,
                                  std::list<scheduled_point_t *> &list)
{
    scheduled_point_t *point = ctx->plan->sp_tree_get_state (at);
    while (point) {
        if (point->at >= static_cast<int64_t> (at + duration))
            break;
        else if (point->at >= at)
            list.push_back (point);
        point = ctx->plan->sp_tree_next (point);
    }
}

static int update_points_add_span (planner_t *ctx,
                                   std::list<scheduled_point_t *> &list,
                                   std::shared_ptr<span_t> &span)
{
    int rc = 0;
    for (auto &point : list) {
        point->scheduled += span->planned;
        point->remaining -= span->planned;
        if ((point->scheduled > ctx->plan->get_total_resources ()) || (point->remaining < 0)) {
            errno = ERANGE;
            rc = -1;
        }
    }
    return rc;
}

static int update_points_subtract_span (planner_t *ctx,
                                        std::list<scheduled_point_t *> &list,
                                        std::shared_ptr<span_t> &span)
{
    int rc = 0;
    for (auto &point : list) {
        point->scheduled -= span->planned;
        point->remaining += span->planned;
        if ((point->scheduled < 0) || (point->remaining > ctx->plan->get_total_resources ())) {
            errno = ERANGE;
            rc = -1;
        }
    }
    return rc;
}

static int update_points_partial_subtract_span (planner_t *ctx,
                                                std::list<scheduled_point_t *> &list,
                                                int64_t to_remove)
{
    for (auto &point : list) {
        point->scheduled -= to_remove;
        point->remaining += to_remove;
        if ((point->scheduled < 0) || (point->remaining > ctx->plan->get_total_resources ())) {
            errno = ERANGE;
            return -1;
        }
    }
    return 0;
}

static bool span_ok (planner_t *ctx,
                     scheduled_point_t *start_point,
                     uint64_t duration,
                     int64_t request)
{
    bool ok = true;
    scheduled_point_t *next_point = nullptr;
    for (next_point = start_point; next_point != nullptr;
         next_point = ctx->plan->sp_tree_next (next_point)) {
        if (next_point->at >= (start_point->at + (int64_t)duration)) {
            ok = true;
            break;
        } else if (request > next_point->remaining) {
            ctx->plan->mt_tree_remove (start_point);
            track_points (ctx->plan->get_avail_time_iter (), start_point);
            ok = false;
            break;
        }
    }
    return ok;
}

static int64_t avail_at (planner_t *ctx, int64_t on_or_after, uint64_t duration, int64_t request)
{
    int64_t at = -1;
    scheduled_point_t *start_point = nullptr;
    while ((start_point = ctx->plan->mt_tree_get_mintime (request))) {
        at = start_point->at;
        if (at < on_or_after) {
            ctx->plan->mt_tree_remove (start_point);
            track_points (ctx->plan->get_avail_time_iter (), start_point);
            at = -1;

        } else if (span_ok (ctx, start_point, duration, request)) {
            ctx->plan->mt_tree_remove (start_point);
            track_points (ctx->plan->get_avail_time_iter (), start_point);
            if (static_cast<int64_t> (at + duration) > ctx->plan->get_plan_end ())
                at = -1;
            break;
        }
    }
    return at;
}

static int64_t unavail_at (planner_t *ctx, int64_t at, const int64_t request)
{
    if (at > ctx->plan->get_plan_end ()) {
        errno = ERANGE;
        return -1;
    }

    scheduled_point_t *point = ctx->plan->sp_tree_get_state (at);
    while (point) {
        if (request > point->remaining) {
            return point->at;
        }
        point = ctx->plan->sp_tree_next (point);
    }
    return -1;
}

static bool avail_during (planner_t *ctx, int64_t at, uint64_t duration, const int64_t request)
{
    bool ok = true;
    if (static_cast<int64_t> (at + duration) > ctx->plan->get_plan_end ()) {
        errno = ERANGE;
        return -1;
    }

    scheduled_point_t *point = ctx->plan->sp_tree_get_state (at);
    while (point) {
        if (point->at >= (at + (int64_t)duration)) {
            ok = true;
            break;
        } else if (request > point->remaining) {
            ok = false;
            break;
        }
        point = ctx->plan->sp_tree_next (point);
    }
    return ok;
}

static scheduled_point_t *avail_resources_during (planner_t *ctx, int64_t at, uint64_t duration)
{
    if (static_cast<int64_t> (at + duration) > ctx->plan->get_plan_end ()) {
        errno = ERANGE;
        return nullptr;
    }

    scheduled_point_t *point = ctx->plan->sp_tree_get_state (at);
    scheduled_point_t *min = point;
    while (point) {
        if (point->at >= (at + (int64_t)duration))
            break;
        else if (min->remaining > point->remaining)
            min = point;
        point = ctx->plan->sp_tree_next (point);
    }
    return min;
}

////////////////////////////////////////////////////////////////////////////////
// Utilities
////////////////////////////////////////////////////////////////////////////////

static inline void erase (planner_t *ctx)
{
    ctx->plan->erase ();
}

static inline bool not_feasible (planner_t *ctx,
                                 int64_t start_time,
                                 uint64_t duration,
                                 int64_t request)
{
    bool rc = (start_time < ctx->plan->get_plan_start ()) || (duration < 1)
              || (static_cast<int64_t> (start_time + duration - 1) > ctx->plan->get_plan_end ());
    return rc;
}

static int span_input_check (planner_t *ctx, int64_t start_time, uint64_t duration, int64_t request)
{
    int rc = -1;
    if (!ctx || not_feasible (ctx, start_time, duration, request)) {
        errno = EINVAL;
        goto done;
    } else if (request > ctx->plan->get_total_resources () || request < 0) {
        errno = ERANGE;
        goto done;
    }
    rc = 0;
done:
    return rc;
}

static std::shared_ptr<span_t> span_new (planner_t *ctx,
                                         int64_t start_time,
                                         uint64_t duration,
                                         uint64_t request)
{
    std::shared_ptr<span_t> span = nullptr;
    try {
        if (span_input_check (ctx, start_time, duration, (int64_t)request) == -1)
            goto done;
        ctx->plan->incr_span_counter ();
        if (ctx->plan->get_span_lookup ().find (ctx->plan->get_span_counter ())
            != ctx->plan->get_span_lookup ().end ()) {
            errno = EEXIST;
            goto done;
        }
        span = std::make_shared<span_t> ();
        span->start = start_time;
        span->last = start_time + duration;
        span->span_id = ctx->plan->get_span_counter ();
        span->planned = request;
        span->in_system = 0;
        span->start_p = nullptr;
        span->last_p = nullptr;

        // errno = EEXIST condition already checked above
        ctx->plan->span_lookup_insert (span->span_id, span);
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
    }

done:
    return span;
}

////////////////////////////////////////////////////////////////////////////////
// PUBLIC PLANNER API
////////////////////////////////////////////////////////////////////////////////

extern "C" planner_t *planner_new (int64_t base_time,
                                   uint64_t duration,
                                   uint64_t resource_totals,
                                   const char *resource_type)
{
    planner_t *ctx = nullptr;

    if (duration < 1 || !resource_type) {
        errno = EINVAL;
        goto done;
    } else if (resource_totals > static_cast<uint64_t> (std::numeric_limits<int64_t>::max ())) {
        errno = ERANGE;
        goto done;
    }

    try {
        ctx = new planner_t (base_time, duration, resource_totals, resource_type);
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
    }

done:
    return ctx;
}

extern "C" planner_t *planner_copy (planner_t *p)
{
    planner_t *ctx = nullptr;

    try {
        ctx = new planner_t (*(p->plan));
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
    }

    return ctx;
}

extern "C" void planner_assign (planner_t *lhs, planner_t *rhs)
{
    (*(lhs->plan) = *(rhs->plan));
}

extern "C" planner_t *planner_new_empty ()
{
    planner_t *ctx = nullptr;

    try {
        ctx = new planner_t ();
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
    }

    return ctx;
}

extern "C" int planner_reset (planner_t *ctx, int64_t base_time, uint64_t duration)
{
    int rc = 0;
    if (!ctx || duration < 1) {
        errno = EINVAL;
        return -1;
    }

    erase (ctx);
    try {
        // Returns 0 or a negative int
        rc = ctx->plan->reinitialize (base_time, duration);
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        rc = -1;
    }
    return rc;
}

extern "C" void planner_destroy (planner_t **ctx_p)
{
    if (ctx_p && *ctx_p) {
        delete *ctx_p;
        *ctx_p = nullptr;
    }
}

extern "C" int64_t planner_base_time (planner_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    return ctx->plan->get_plan_start ();
}

extern "C" int64_t planner_duration (planner_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    return ctx->plan->get_plan_end () - ctx->plan->get_plan_start ();
}

extern "C" int64_t planner_resource_total (planner_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    return ctx->plan->get_total_resources ();
}

extern "C" const char *planner_resource_type (planner_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return nullptr;
    }
    return ctx->plan->get_resource_type ().c_str ();
}

extern "C" int64_t planner_avail_time_first (planner_t *ctx,
                                             int64_t on_or_after,
                                             uint64_t duration,
                                             uint64_t request)
{
    int64_t t = -1;
    if (!ctx || on_or_after < ctx->plan->get_plan_start ()
        || on_or_after >= ctx->plan->get_plan_end () || duration < 1) {
        errno = EINVAL;
        return -1;
    }
    if (static_cast<int64_t> (request) > ctx->plan->get_total_resources ()) {
        errno = ERANGE;
        return -1;
    }
    restore_track_points (ctx);
    ctx->plan->set_avail_time_iter_set (1);
    copy_req (ctx->plan->get_current_request (), on_or_after, duration, request);
    if ((t = avail_at (ctx, on_or_after, duration, (int64_t)request)) == -1)
        errno = ENOENT;
    return t;
}

extern "C" int64_t planner_avail_time_next (planner_t *ctx)
{
    int64_t t = -1;
    int64_t on_or_after = -1;
    uint64_t duration = 0;
    int64_t request_count = 0;
    if (!ctx || !ctx->plan->get_avail_time_iter_set ()) {
        errno = EINVAL;
        return -1;
    }
    request_count = ctx->plan->get_current_request_const ().count;
    on_or_after = ctx->plan->get_current_request_const ().on_or_after;
    duration = ctx->plan->get_current_request_const ().duration;
    if (request_count > ctx->plan->get_total_resources ()) {
        errno = ERANGE;
        return -1;
    }
    if ((t = avail_at (ctx, on_or_after, duration, (int64_t)request_count)) == -1)
        errno = ENOENT;
    return t;
}

extern "C" int planner_avail_during (planner_t *ctx,
                                     int64_t start_time,
                                     uint64_t duration,
                                     uint64_t request)
{
    bool ok = false;
    if (!ctx || duration < 1) {
        errno = EINVAL;
        return -1;
    }
    if (static_cast<int64_t> (request) > ctx->plan->get_total_resources ()) {
        errno = ERANGE;
        return -1;
    }
    ok = avail_during (ctx, start_time, duration, (int64_t)request);
    return ok ? 0 : -1;
}

extern "C" int64_t planner_avail_resources_during (planner_t *ctx, int64_t at, uint64_t duration)
{
    scheduled_point_t *min_point = nullptr;
    if (!ctx || at > ctx->plan->get_plan_end () || duration < 1) {
        errno = EINVAL;
        return -1;
    }
    min_point = avail_resources_during (ctx, at, duration);
    return min_point->remaining;
}

extern "C" int64_t planner_avail_resources_at (planner_t *ctx, int64_t at)
{
    const scheduled_point_t *state = nullptr;
    if (!ctx || at > ctx->plan->get_plan_end () || at < ctx->plan->get_plan_start ()) {
        errno = EINVAL;
        return -1;
    }
    state = ctx->plan->sp_tree_get_state (at);
    return state->remaining;
}

extern "C" int64_t planner_unavail_time_first (planner_t *ctx,
                                               int64_t on_or_after,
                                               uint64_t request)
{
    int64_t t = -1;
    if (!ctx || on_or_after < ctx->plan->get_plan_start ()
        || on_or_after >= ctx->plan->get_plan_end ()) {
        errno = EINVAL;
        return -1;
    }
    if (static_cast<int64_t> (request) > ctx->plan->get_total_resources ()) {
        errno = ERANGE;
        return -1;
    }
    restore_track_points (ctx);
    ctx->plan->set_avail_time_iter_set (1);
    copy_req (ctx->plan->get_current_request (), on_or_after, 0, request);
    if ((t = unavail_at (ctx, on_or_after, (int64_t)request)) == -1)
        errno = ENOENT;
    return t;
}

extern "C" int64_t planner_add_span (planner_t *ctx,
                                     int64_t start_time,
                                     uint64_t duration,
                                     uint64_t request)
{
    std::shared_ptr<span_t> span = nullptr;
    scheduled_point_t *start_point = nullptr;
    scheduled_point_t *last_point = nullptr;

    if (!avail_during (ctx, start_time, duration, (int64_t)request)) {
        errno = EINVAL;
        return -1;
    }
    if (!(span = span_new (ctx, start_time, duration, request)))
        return -1;

    restore_track_points (ctx);
    std::list<scheduled_point_t *> list;
    if ((start_point = get_or_new_point (ctx, span->start)) == nullptr)
        return -1;
    start_point->ref_count++;
    if ((last_point = get_or_new_point (ctx, span->last)) == nullptr)
        return -1;
    last_point->ref_count++;

    fetch_overlap_points (ctx, span->start, duration, list);
    update_points_add_span (ctx, list, span);

    start_point->new_point = 0;
    span->start_p = start_point;
    last_point->new_point = 0;
    span->last_p = last_point;

    update_mintime_resource_tree (ctx, list);

    list.clear ();
    span->in_system = 1;
    ctx->plan->set_avail_time_iter_set (0);

    return span->span_id;
}

extern "C" int planner_rem_span (planner_t *ctx, int64_t span_id)
{
    int rc = -1;
    uint64_t duration = 0;
    std::map<int64_t, std::shared_ptr<span_t>>::iterator it;

    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    it = ctx->plan->get_span_lookup ().find (span_id);
    if (it == ctx->plan->get_span_lookup ().end ()) {
        errno = EINVAL;
        return -1;
    }
    std::shared_ptr<span_t> &span = it->second;

    restore_track_points (ctx);
    std::list<scheduled_point_t *> list;
    duration = span->last - span->start;
    span->start_p->ref_count--;
    span->last_p->ref_count--;
    fetch_overlap_points (ctx, span->start, duration, list);
    update_points_subtract_span (ctx, list, span);
    update_mintime_resource_tree (ctx, list);
    span->in_system = 0;

    if (span->start_p->ref_count == 0) {
        ctx->plan->sp_tree_remove (span->start_p);
        if (span->start_p->in_mt_resource_tree)
            ctx->plan->mt_tree_remove (span->start_p);
        delete span->start_p;
        span->start_p = nullptr;
    }
    if (span->last_p->ref_count == 0) {
        ctx->plan->sp_tree_remove (span->last_p);
        if (span->last_p->in_mt_resource_tree)
            ctx->plan->mt_tree_remove (span->last_p);
        delete span->last_p;
        span->last_p = nullptr;
    }
    ctx->plan->span_lookup_erase (it);
    list.clear ();
    ctx->plan->set_avail_time_iter_set (0);
    rc = 0;

    return rc;
}

extern "C" int planner_reduce_span (planner_t *ctx,
                                    int64_t span_id,
                                    int64_t to_remove,
                                    bool &removed)
{
    int rc = -1;
    uint64_t duration = 0;
    std::map<int64_t, std::shared_ptr<span_t>>::iterator it;

    removed = false;
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    it = ctx->plan->get_span_lookup ().find (span_id);
    if (it == ctx->plan->get_span_lookup ().end ()) {
        errno = EINVAL;
        return -1;
    }
    std::shared_ptr<span_t> &span = it->second;
    // Planned values can be 0 (especially when part of planner_multi),
    // and to_remove may be zero as well. We want to remove spans where
    // Planned values and to_remove are identically zero.
    if (to_remove == span->planned) {
        // Removing the whole span
        rc = planner_rem_span (ctx, span_id);
        removed = true;
    } else if (to_remove == 0) {
        rc = 0;
    } else if (to_remove < span->planned) {
        // Removing partial span resources
        restore_track_points (ctx);
        span->planned -= to_remove;
        std::list<scheduled_point_t *> list;
        duration = span->last - span->start;
        fetch_overlap_points (ctx, span->start, duration, list);
        update_points_partial_subtract_span (ctx, list, to_remove);
        update_mintime_resource_tree (ctx, list);
        rc = 0;
    } else {
        // Error; rc already -1
        errno = EINVAL;
    }

    return rc;
}

extern "C" int64_t planner_span_first (planner_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    auto span_lookup_begin = ctx->plan->get_span_lookup ().begin ();
    ctx->plan->set_span_lookup_iter (span_lookup_begin);
    if (ctx->plan->get_span_lookup_iter () == ctx->plan->get_span_lookup ().end ()) {
        errno = EINVAL;
        return -1;
    }
    std::shared_ptr<span_t> &span = ctx->plan->get_span_lookup_iter ()->second;
    return span->span_id;
}

extern "C" int64_t planner_span_next (planner_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    ctx->plan->incr_span_lookup_iter ();
    if (ctx->plan->get_span_lookup_iter () == ctx->plan->get_span_lookup ().end ()) {
        errno = EINVAL;
        return -1;
    }
    std::shared_ptr<span_t> &span = ctx->plan->get_span_lookup_iter ()->second;
    return span->span_id;
}

extern "C" size_t planner_span_size (planner_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return 0;
    }
    return ctx->plan->span_lookup_get_size ();
}

extern "C" bool planner_is_active_span (planner_t *ctx, int64_t span_id)
{
    if (!ctx) {
        errno = EINVAL;
        return false;
    }
    auto it = ctx->plan->get_span_lookup ().find (span_id);
    if (ctx->plan->get_span_lookup ().find (span_id) == ctx->plan->get_span_lookup ().end ()) {
        errno = EINVAL;
        return false;
    }
    std::shared_ptr<span_t> &span = it->second;
    return (span->in_system) ? true : false;
}

extern "C" int64_t planner_span_start_time (planner_t *ctx, int64_t span_id)
{
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    auto it = ctx->plan->get_span_lookup ().find (span_id);
    if (ctx->plan->get_span_lookup ().find (span_id) == ctx->plan->get_span_lookup ().end ()) {
        errno = EINVAL;
        return -1;
    }
    std::shared_ptr<span_t> &span = it->second;
    return span->start;
}

extern "C" int64_t planner_span_duration (planner_t *ctx, int64_t span_id)
{
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    auto it = ctx->plan->get_span_lookup ().find (span_id);
    if (ctx->plan->get_span_lookup ().find (span_id) == ctx->plan->get_span_lookup ().end ()) {
        errno = EINVAL;
        return -1;
    }
    std::shared_ptr<span_t> &span = it->second;
    return (span->last - span->start);
}

extern "C" int64_t planner_span_resource_count (planner_t *ctx, int64_t span_id)
{
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    auto it = ctx->plan->get_span_lookup ().find (span_id);
    if (ctx->plan->get_span_lookup ().find (span_id) == ctx->plan->get_span_lookup ().end ()) {
        errno = EINVAL;
        return -1;
    }
    std::shared_ptr<span_t> &span = it->second;
    return span->planned;
}

extern "C" bool planners_equal (planner_t *lhs, planner_t *rhs)
{
    return (*(lhs->plan) == *(rhs->plan));
}

extern "C" int planner_update_total (planner_t *ctx, uint64_t resource_total)
{
    return ctx->plan->update_total (resource_total);
}

/*
 * vi: ts=4 sw=4 expandtab
 */
