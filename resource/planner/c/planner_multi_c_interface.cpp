/*****************************************************************************\
 * Copyright 2023 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <limits>
#include <vector>
#include <map>
#include <numeric>

#include "planner_multi.h"
#include "resource/planner/c++/planner_multi.hpp"

////////////////////////////////////////////////////////////////////////////////
// Planner Multi and Resource Update APIs
////////////////////////////////////////////////////////////////////////////////

static void fill_iter_request (planner_multi_t *ctx,
                               struct request_multi *iter,
                               int64_t at,
                               uint64_t duration,
                               const uint64_t *resources,
                               size_t len)
{
    size_t i;
    iter->on_or_after = at;
    iter->duration = duration;
    for (i = 0; i < len; ++i)
        iter->counts[ctx->plan_multi->get_resource_type_at (i)] = resources[i];
}

extern "C" planner_multi_t *planner_multi_new (int64_t base_time,
                                               uint64_t duration,
                                               const uint64_t *resource_totals,
                                               const char **resource_types,
                                               size_t len)
{
    size_t i = 0;
    planner_multi_t *ctx = nullptr;

    if (duration < 1 || !resource_totals || !resource_types) {
        errno = EINVAL;
        goto error;
    } else {
        for (i = 0; i < len; ++i) {
            if (resource_totals[i] > static_cast<uint64_t> (std::numeric_limits<int64_t>::max ())) {
                errno = ERANGE;
                goto error;
            }
        }
    }

    try {
        ctx = new planner_multi_t (base_time, duration, resource_totals, resource_types, len);
    } catch (std::bad_alloc &e) {
        goto nomem_error;
    }
    return ctx;

nomem_error:
    errno = ENOMEM;
    planner_multi_destroy (&ctx);
error:
    return ctx;
}

extern "C" planner_multi_t *planner_multi_empty ()
{
    planner_multi_t *ctx = nullptr;

    try {
        ctx = new planner_multi_t ();
    } catch (std::bad_alloc &e) {
        goto nomem_error;
    }
    return ctx;

nomem_error:
    errno = ENOMEM;
    planner_multi_destroy (&ctx);
    return ctx;
}

extern "C" planner_multi_t *planner_multi_copy (planner_multi_t *mp)
{
    planner_multi_t *ctx = nullptr;

    try {
        ctx = new planner_multi_t (*(mp->plan_multi));
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        goto nomem_error;
    }
    return ctx;

nomem_error:
    errno = ENOMEM;
    planner_multi_destroy (&ctx);
    return ctx;
}

extern "C" void planner_multi_assign (planner_multi_t *lhs, planner_multi_t *rhs)
{
    (*(lhs->plan_multi) = *(rhs->plan_multi));
}

extern "C" int64_t planner_multi_base_time (planner_multi_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    return planner_base_time (ctx->plan_multi->get_planner_at (static_cast<size_t> (0)));
}

extern "C" int64_t planner_multi_duration (planner_multi_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    return planner_duration (ctx->plan_multi->get_planner_at (static_cast<size_t> (0)));
}

extern "C" size_t planner_multi_resources_len (planner_multi_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return 0;
    }
    return ctx->plan_multi->get_planners_size ();
}

extern "C" const char *planner_multi_resource_type_at (planner_multi_t *ctx, unsigned int i)
{
    if (!ctx) {
        errno = EINVAL;
        return nullptr;
    }
    return ctx->plan_multi->get_resource_type_at (i);
}

extern "C" int64_t planner_multi_resource_total_at (planner_multi_t *ctx, unsigned int i)
{
    int64_t rc = -1;
    if (ctx) {
        if (i >= ctx->plan_multi->get_planners_size ()) {
            errno = EINVAL;
            goto done;
        }
        rc = ctx->plan_multi->get_resource_total_at (i);
    }
done:
    return rc;
}

extern "C" int64_t planner_multi_resource_total_by_type (planner_multi_t *ctx,
                                                         const char *resource_type)
{
    int64_t rc = -1;
    if (!ctx || !resource_type)
        goto done;

    rc = ctx->plan_multi->get_resource_total_at (resource_type);
    if (rc == -1)
        errno = EINVAL;
done:
    return rc;
}

extern "C" int planner_multi_reset (planner_multi_t *ctx, int64_t base_time, uint64_t duration)
{
    size_t i = 0;
    int rc = -1;
    if (!ctx || duration < 1) {
        errno = EINVAL;
        goto done;
    }

    for (i = 0; i < ctx->plan_multi->get_planners_size (); ++i)
        if (planner_reset (ctx->plan_multi->get_planner_at (i), base_time, duration) == -1)
            goto done;

    rc = 0;
done:
    return rc;
}

extern "C" void planner_multi_destroy (planner_multi_t **ctx_p)
{
    if (ctx_p && *ctx_p) {
        delete *ctx_p;
        *ctx_p = nullptr;
    }
}

extern "C" planner_t *planner_multi_planner_at (planner_multi_t *ctx, unsigned int i)
{
    planner_t *planner = nullptr;
    if (!ctx || i >= ctx->plan_multi->get_planners_size ()) {
        errno = EINVAL;
        goto done;
    }
    planner = ctx->plan_multi->get_planner_at (i);
done:
    return planner;
}

extern "C" int64_t planner_multi_avail_time_first (planner_multi_t *ctx,
                                                   int64_t on_or_after,
                                                   uint64_t duration,
                                                   const uint64_t *resource_requests,
                                                   size_t len)
{
    size_t i = 0;
    int unmet = 0;
    int64_t t = -1;

    if (!ctx || !resource_requests || ctx->plan_multi->get_planners_size () < 1
        || ctx->plan_multi->get_planners_size () != len) {
        errno = EINVAL;
        goto done;
    }

    fill_iter_request (ctx,
                       &(ctx->plan_multi->get_iter ()),
                       on_or_after,
                       duration,
                       resource_requests,
                       len);

    if ((t = planner_avail_time_first (ctx->plan_multi->get_planner_at (static_cast<size_t> (0)),
                                       on_or_after,
                                       duration,
                                       resource_requests[0]))
        == -1)
        goto done;

    do {
        unmet = 0;
        for (i = 1; i < ctx->plan_multi->get_planners_size (); ++i) {
            if ((unmet = planner_avail_during (ctx->plan_multi->get_planner_at (i),
                                               t,
                                               duration,
                                               resource_requests[i]))
                == -1)
                break;
        }
    } while (
        unmet
        && (t = planner_avail_time_next (ctx->plan_multi->get_planner_at (static_cast<size_t> (0))))
               != -1);

done:
    return t;
}

extern "C" int64_t planner_multi_avail_time_next (planner_multi_t *ctx)
{
    size_t i = 0;
    int unmet = 0;
    int64_t t = -1;
    std::string type;

    if (!ctx) {
        errno = EINVAL;
        goto done;
    }
    do {
        unmet = 0;
        if ((t = planner_avail_time_next (
                 ctx->plan_multi->get_planner_at (static_cast<size_t> (0))))
            == -1)
            break;
        for (i = 1; i < ctx->plan_multi->get_planners_size (); ++i) {
            type = ctx->plan_multi->get_resource_type_at (i);
            if ((unmet = planner_avail_during (ctx->plan_multi->get_planner_at (i),
                                               t,
                                               ctx->plan_multi->get_iter ().duration,
                                               ctx->plan_multi->get_iter ().counts.at (type)))
                == -1)
                break;
        }
    } while (unmet);

done:
    return t;
}

extern "C" int64_t planner_multi_avail_resources_at (planner_multi_t *ctx,
                                                     int64_t at,
                                                     unsigned int i)
{
    if (!ctx || i >= ctx->plan_multi->get_planners_size ()) {
        errno = EINVAL;
        return -1;
    }
    return planner_avail_resources_at (ctx->plan_multi->get_planner_at (i), at);
}

extern "C" int planner_multi_avail_resources_array_at (planner_multi_t *ctx,
                                                       int64_t at,
                                                       int64_t *resource_counts,
                                                       size_t len)
{
    size_t i = 0;
    int64_t rc = 0;
    if (!ctx || !resource_counts || ctx->plan_multi->get_planners_size () != len) {
        errno = EINVAL;
        return -1;
    }
    for (i = 0; i < ctx->plan_multi->get_planners_size (); ++i) {
        rc = planner_avail_resources_at (ctx->plan_multi->get_planner_at (i), at);
        if (rc == -1)
            break;
        resource_counts[i] = rc;
    }
    return (rc == -1) ? -1 : 0;
}

extern "C" int planner_multi_avail_during (planner_multi_t *ctx,
                                           int64_t at,
                                           uint64_t duration,
                                           const uint64_t *resource_requests,
                                           size_t len)
{
    size_t i = 0;
    int rc = 0;
    if (!ctx || !resource_requests || ctx->plan_multi->get_planners_size () != len) {
        errno = EINVAL;
        return -1;
    }
    for (i = 0; i < ctx->plan_multi->get_planners_size (); ++i) {
        rc = planner_avail_during (ctx->plan_multi->get_planner_at (i),
                                   at,
                                   duration,
                                   resource_requests[i]);
        if (rc == -1)
            break;
    }
    return rc;
}

extern "C" int planner_multi_avail_resources_array_during (planner_multi_t *ctx,
                                                           int64_t at,
                                                           uint64_t duration,
                                                           int64_t *resource_counts,
                                                           size_t len)
{
    size_t i = 0;
    int64_t rc = 0;
    if (!ctx || !resource_counts || ctx->plan_multi->get_planners_size () < 1
        || ctx->plan_multi->get_planners_size () != len) {
        errno = EINVAL;
        return -1;
    }
    for (i = 0; i < ctx->plan_multi->get_planners_size (); ++i) {
        rc = planner_avail_resources_during (ctx->plan_multi->get_planner_at (i), at, duration);
        if (rc == -1)
            break;
        resource_counts[i] = rc;
    }
    return (rc == -1) ? -1 : 0;
}

extern "C" int64_t planner_multi_add_span (planner_multi_t *ctx,
                                           int64_t start_time,
                                           uint64_t duration,
                                           const uint64_t *resource_requests,
                                           size_t len)
{
    size_t i = 0;
    int64_t span = -1;
    int64_t mspan = -1;

    if (!ctx || !resource_requests || len != ctx->plan_multi->get_planners_size ())
        return -1;

    mspan = ctx->plan_multi->get_span_counter ();
    auto res = ctx->plan_multi->get_span_lookup ().insert (
        std::pair<int64_t, std::vector<int64_t>> (mspan, std::vector<int64_t> ()));
    if (!res.second) {
        errno = EEXIST;
        return -1;
    }

    ctx->plan_multi->incr_span_counter ();

    for (i = 0; i < len; ++i) {
        if ((span = planner_add_span (ctx->plan_multi->get_planner_at (i),
                                      start_time,
                                      duration,
                                      resource_requests[i]))
            == -1) {
            ctx->plan_multi->get_span_lookup ().erase (mspan);
            return -1;
        }
        ctx->plan_multi->get_span_lookup ()[mspan].push_back (span);
    }
    return mspan;
}

extern "C" int planner_multi_rem_span (planner_multi_t *ctx, int64_t span_id)
{
    size_t i;
    int rc = -1;

    if (!ctx || span_id < 0) {
        errno = EINVAL;
        return -1;
    }
    auto it = ctx->plan_multi->get_span_lookup ().find (span_id);
    if (it == ctx->plan_multi->get_span_lookup ().end ()) {
        errno = ENOENT;
        goto done;
    }
    for (i = 0; i < it->second.size (); ++i) {
        // If executed after partial cancel, depending on pruning filter settings
        // some spans may no longer exist. In that case the span_lookup value for
        // the resource type will be -1.
        if (it->second[i] != -1) {
            if (planner_rem_span (ctx->plan_multi->get_planner_at (i), it->second[i]) == -1)
                goto done;
        }
    }
    ctx->plan_multi->get_span_lookup ().erase (it);
    rc = 0;
done:
    return rc;
}

extern "C" int planner_multi_reduce_span (planner_multi_t *ctx,
                                          int64_t span_id,
                                          const uint64_t *reduced_totals,
                                          const char **resource_types,
                                          size_t len,
                                          bool &removed)
{
    size_t i = 0;
    int rc = -1;
    bool tmp_removed = false;
    size_t mspan_idx;
    int64_t mspan_sum = 0;
    std::set<size_t> ext_res_types;

    removed = false;
    if (!ctx || span_id < 0 || !reduced_totals || !resource_types) {
        errno = EINVAL;
        return -1;
    }
    auto span_it = ctx->plan_multi->get_span_lookup ().find (span_id);
    if (span_it == ctx->plan_multi->get_span_lookup ().end ()) {
        errno = ENOENT;
        return -1;
    }
    for (i = 0; i < len; ++i) {
        if (reduced_totals[i] > static_cast<uint64_t> (std::numeric_limits<int64_t>::max ())) {
            errno = ERANGE;
            return -1;
        }
        // Index could be different than the span_lookup due to order of
        // iteration in the reader differing from the graph initialization
        // order.
        mspan_idx = ctx->plan_multi->get_resource_type_idx (resource_types[i]);
        // Resource type not found; can happen if agfilter doesn't track resource
        if (mspan_idx >= ctx->plan_multi->get_planners_size ())
            continue;

        tmp_removed = false;
        if ((rc = planner_reduce_span (ctx->plan_multi->get_planner_at (mspan_idx),
                                       span_it->second.at (mspan_idx),
                                       reduced_totals[i],
                                       tmp_removed))
            == -1) {
            // Could return -1 if the span with 0 resource request had been removed
            // by a previous cancellation, so need to check if the span exists.
            if (planner_is_active_span (ctx->plan_multi->get_planner_at (mspan_idx),
                                        span_it->second.at (mspan_idx))) {
                // We know the span is valid, so planner_reduce_span
                // encountered another error.
                errno = EINVAL;
                goto error;
            }
        }
        ext_res_types.insert (mspan_idx);
        // Enter invalid span ID in the span_lookup to indicate the resource
        // removal.
        if (tmp_removed)
            span_it->second[mspan_idx] = -1;
    }
    // Iterate over planner_multi resources since resource_types may not cover
    // all planner_multi resources. If resource_types contains fewer types
    // than the total planner_multi resources, this means the reader partial
    // cancel didn't encounter those resource types. This can happen since
    // agfilter requests for 0 resources are entered for resource types
    // tracked by the agfilter that the job didn't request. Ex: job requests
    // cores, but the agfilter tracks cores and memory. A span will be created
    // for cores and memory, but the memory request will be 0. We need to
    // remove these spans.
    for (i = 0; i < ctx->plan_multi->get_planners_size (); ++i) {
        tmp_removed = false;
        // Check if the resource type was already processed in a previous
        // loop.
        if (ext_res_types.find (i) == ext_res_types.end ()) {
            if ((rc = planner_reduce_span (ctx->plan_multi->get_planner_at (i),
                                           span_it->second.at (i),
                                           0,
                                           tmp_removed))
                == -1) {
                // Could return -1 if the span with 0 resource request had been
                // removed by a previous cancellation, so need to check if the
                // span exists.
                if (planner_is_active_span (ctx->plan_multi->get_planner_at (i),
                                            span_it->second.at (i))) {
                    // We know the span is valid, so planner_reduce_span
                    // encountered another error.
                    errno = EINVAL;
                    goto error;
                }
            }
            // Enter invalid span ID in the span_lookup to indicate the
            // resource removal.
            if (tmp_removed)
                span_it->second[i] = -1;
        }
    }
    mspan_sum = std::accumulate (span_it->second.begin (),
                                 span_it->second.end (),
                                 0,
                                 std::plus<int64_t> ());
    // Delete if all entries are -1
    if (mspan_sum == (-1 * span_it->second.size ())) {
        ctx->plan_multi->get_span_lookup ().erase (span_it);
        removed = true;
    }

    rc = 0;
error:
    return rc;
}

extern "C" int64_t planner_multi_span_planned_at (planner_multi_t *ctx,
                                                  int64_t span_id,
                                                  unsigned int i)
{
    if (!ctx || span_id < 0) {
        errno = EINVAL;
        return -1;
    }
    auto span_it = ctx->plan_multi->get_span_lookup ().find (span_id);
    if (span_it == ctx->plan_multi->get_span_lookup ().end ()) {
        errno = ENOENT;
        return -1;
    }
    return planner_span_resource_count (ctx->plan_multi->get_planner_at (i),
                                        span_it->second.at (i));
}

extern "C" int64_t planner_multi_span_first (planner_multi_t *ctx)
{
    int64_t rc = -1;
    std::map<uint64_t, std::vector<int64_t>>::iterator tmp_it =
        ctx->plan_multi->get_span_lookup ().begin ();
    if (!ctx) {
        errno = EINVAL;
        goto done;
    }
    ctx->plan_multi->set_span_lookup_iter (tmp_it);
    if (ctx->plan_multi->get_span_lookup_iter () == ctx->plan_multi->get_span_lookup ().end ()) {
        errno = ENOENT;
        goto done;
    }
    rc = ctx->plan_multi->get_span_lookup_iter ()->first;
done:
    return rc;
}

extern "C" int64_t planner_multi_span_next (planner_multi_t *ctx)
{
    int64_t rc = -1;
    if (!ctx) {
        errno = EINVAL;
        goto done;
    }
    ctx->plan_multi->incr_span_lookup_iter ();
    if (ctx->plan_multi->get_span_lookup_iter () == ctx->plan_multi->get_span_lookup ().end ()) {
        errno = ENOENT;
        goto done;
    }
    rc = ctx->plan_multi->get_span_lookup_iter ()->first;
done:
    return rc;
}

extern "C" size_t planner_multi_span_size (planner_multi_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return 0;
    }
    return ctx->plan_multi->get_span_lookup ().size ();
}

extern "C" bool planner_multis_equal (planner_multi_t *lhs, planner_multi_t *rhs)
{
    return (*(lhs->plan_multi) == *(rhs->plan_multi));
}

extern "C" int planner_multi_update (planner_multi_t *ctx,
                                     const uint64_t *resource_totals,
                                     const char **resource_types,
                                     size_t len)
{
    int rc = -1;
    size_t i = 0;
    // Assuming small number of resource types,
    // could try set, too
    std::unordered_set<std::string> rtypes;
    int64_t base_time = 0;
    int64_t duration = 0;

    if (!ctx || !resource_totals || !resource_types) {
        errno = EINVAL;
        goto done;
    }
    base_time = planner_base_time (ctx->plan_multi->get_planner_at (static_cast<size_t> (0)));
    duration = planner_duration (ctx->plan_multi->get_planner_at (static_cast<size_t> (0)));
    if (duration < 0) {
        errno = EINVAL;
        goto done;
    }

    for (i = 0; i < len; ++i) {
        if (resource_totals[i] > static_cast<uint64_t> (std::numeric_limits<int64_t>::max ())) {
            errno = ERANGE;
            goto done;
        }
        rtypes.insert (resource_types[i]);
        if (!ctx->plan_multi->planner_at (resource_types[i])) {
            // Assume base_time same as parent
            ctx->plan_multi->add_planner (base_time,
                                          static_cast<uint64_t> (duration),
                                          resource_totals[i],
                                          resource_types[i],
                                          i);
        } else {
            // Index could have changed
            ctx->plan_multi->update_planner_index (resource_types[i], i);
            if ((rc = ctx->plan_multi->update_planner_total (resource_totals[i], i)) != 0) {
                errno = EINVAL;
                goto done;
            }
        }
    }
    // remove values not in new types
    if (rtypes.size () > 0)
        ctx->plan_multi->delete_planners (rtypes);

    rc = 0;

done:
    return rc;
}

/*
 * vi: ts=4 sw=4 expandtab
 */
