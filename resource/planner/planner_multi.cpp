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

#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <limits>
#include <vector>
#include <map>

#include "planner_multi.h"

struct request {
    int64_t on_or_after;
    uint64_t duration;
    std::vector<int64_t> counts;
};

struct planner_multi {
    std::vector<planner_t *> planners;
    std::vector<uint64_t> resource_totals;
    std::vector<const char *> resource_types;
    struct request iter;
    std::map<uint64_t, std::vector<int64_t>> span_lookup;
    std::map<uint64_t, std::vector<int64_t>>::iterator span_lookup_iter;
    uint64_t span_counter;
};

static void fill_iter_request (planner_multi_t *ctx, struct request *iter,
                               int64_t at, uint64_t duration,
                               const uint64_t *resources, size_t len)
{
    size_t i;
    iter->on_or_after = at;
    iter->duration = duration;
    for (i = 0; i < len; ++i)
        iter->counts[i] = resources[i];
}

extern "C" planner_multi_t *planner_multi_new (
                                int64_t base_time, uint64_t duration,
                                const uint64_t *resource_totals,
                                const char **resource_types, size_t len)
{
    size_t i = 0;
    planner_multi_t *ctx = nullptr;
    char *type = nullptr;
    planner_t *p;

    if (duration < 1 || !resource_totals || !resource_types) {
        errno = EINVAL;
        goto error;
    } else {
        for (i = 0; i < len; ++i) {
            if (resource_totals[i] > std::numeric_limits<int64_t>::max ()) {
                errno = ERANGE;
                goto error;
            }
        }
    }

    try {
        ctx = new planner_multi ();
        ctx->iter.on_or_after = 0;
        ctx->iter.duration = 0;
        for (i = 0; i < len; ++i) {
            ctx->resource_totals.push_back (resource_totals[i]);
            if ( (type = strdup (resource_types[i])) == nullptr)
                goto nomem_error;
            ctx->resource_types.push_back (type);
            ctx->iter.counts.push_back (0);
            if ( (p = planner_new (base_time, duration,
                                   resource_totals[i],
                                   resource_types[i])) == nullptr)
                goto nomem_error;
            ctx->planners.push_back (p);
        }
        ctx->span_counter = 0;
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

extern "C" int64_t planner_multi_base_time (planner_multi_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    return planner_base_time (ctx->planners[0]);
}

extern "C" int64_t planner_multi_duration (planner_multi_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    return planner_duration (ctx->planners[0]);
}

extern "C" size_t planner_multi_resources_len (planner_multi_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return 0;
    }
    return ctx->planners.size ();
}

extern "C" const char **planner_multi_resource_types (planner_multi_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return nullptr;
    }
    return &(ctx->resource_types[0]);
}

extern "C" const uint64_t *planner_multi_resource_totals (planner_multi_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return nullptr;
    }
    return &(ctx->resource_totals[0]);
}

extern "C" int64_t planner_multi_resource_total_at (planner_multi_t *ctx,
                                                    unsigned int i)
{
    int64_t rc = -1;
    if (ctx) {
        if (i >= ctx->planners.size ()) {
            errno = EINVAL;
            goto done;
        }
        rc = planner_resource_total (ctx->planners[i]);
    }
done:
    return rc;
}

extern "C" int64_t planner_multi_resource_total_by_type (
                       planner_multi_t *ctx, const char *resource_type)
{
    size_t i = 0;
    int64_t rc = -1;
    if (!ctx || !resource_type)
        goto done;
    for (i = 0; i < ctx->planners.size (); i++) {
        if ( !(strcmp (ctx->resource_types[i], resource_type))) {
            rc = planner_resource_total (ctx->planners[i]);
            break;
        }
    }
    if (i == ctx->planners.size ())
        errno = EINVAL;
done:
    return rc;
}

extern "C" int planner_multi_reset (planner_multi_t *ctx,
                                    int64_t base_time, uint64_t duration)
{
    size_t i = 0;
    int rc = -1;
    if (!ctx || duration < 1) {
        errno = EINVAL;
        goto done;
    }

    for (i = 0; i < ctx->planners.size (); ++i)
        if (planner_reset (ctx->planners[i], base_time, duration) == -1)
            goto done;

    rc = 0;
done:
    return rc;
}

extern "C" void planner_multi_destroy (planner_multi_t **ctx_p)
{
    size_t i = 0;
    if (ctx_p && *ctx_p) {
        for (i = 0; i < (*ctx_p)->planners.size (); ++i)
            planner_destroy (&((*ctx_p)->planners[i]));
        for (i = 0; i < (*ctx_p)->resource_types.size (); ++i)
            free ((void *)(*ctx_p)->resource_types[i]);
        delete *ctx_p;
        *ctx_p = nullptr;
    }
}

extern "C" planner_t *planner_multi_planner_at (planner_multi_t *ctx,
                                                unsigned int i)
{
    planner_t *planner = nullptr;
    if (!ctx || i >= ctx->planners.size ()) {
        errno = EINVAL;
        goto done;
    }
    planner = ctx->planners[i];
done:
    return planner;
}

extern "C" int64_t planner_multi_avail_time_first (
                       planner_multi_t *ctx, int64_t on_or_after,
                       uint64_t duration,
                       const uint64_t *resource_requests, size_t len)
{
    size_t i = 0;
    int unmet = 0;
    int64_t t = -1;

    if (!ctx || !resource_requests || ctx->planners.size () < 1
         || ctx->planners.size () != len) {
        errno = EINVAL;
        goto done;
    }

    fill_iter_request (ctx, &(ctx->iter),
                       on_or_after, duration, resource_requests, len);

    if ((t = planner_avail_time_first (ctx->planners[0], on_or_after,
                                       duration, resource_requests[0])) == -1)
        goto done;

    do {
        unmet = 0;
        for (i = 1; i < ctx->planners.size (); ++i) {
            if ((unmet = planner_avail_during (ctx->planners[i],
                                               t, duration,
                                               resource_requests[i])) == -1)
                break;
        }
    } while (unmet && (t = planner_avail_time_next (ctx->planners[0])) != -1);

done:
    return t;
}

extern "C" int64_t planner_multi_avail_time_next (planner_multi_t *ctx)
{
    size_t i = 0;
    int unmet = 0;
    int64_t t = -1;

    if (!ctx) {
        errno = EINVAL;
        goto done;
    }

    do {
        unmet = 0;
        if ((t = planner_avail_time_next (ctx->planners[0])) == -1)
            break;
        for (i = 1; i < ctx->planners.size (); ++i) {
            if ((unmet = planner_avail_during (ctx->planners[i], t,
                                               ctx->iter.duration,
                                               ctx->iter.counts[i])) == -1)
                break;
        }
    } while (unmet);

done:
    return t;
}

extern "C" int64_t planner_multi_avail_resources_at (
                       planner_multi_t *ctx, int64_t at, unsigned int i)
{
    if (!ctx || i >= ctx->planners.size ()) {
        errno = EINVAL;
        return -1;
    }
    return planner_avail_resources_at (ctx->planners[i], at);
}

extern "C" int planner_multi_avail_resources_array_at (
                   planner_multi_t *ctx, int64_t at,
                   int64_t *resource_counts, size_t len)
{
    size_t i = 0;
    int64_t rc = 0;
    if (!ctx || !resource_counts || ctx->planners.size () != len) {
        errno = EINVAL;
        return -1;
    }
    for (i = 0; i < ctx->planners.size (); ++i) {
        rc = planner_avail_resources_at (ctx->planners[i], at);
        if (rc == -1)
            break;
        resource_counts[i] = rc;
    }
    return (rc == -1)? -1 : 0;
}

extern "C" int planner_multi_avail_during (
                   planner_multi_t *ctx, int64_t at, uint64_t duration,
                   const uint64_t *resource_requests, size_t len)
{
    size_t i = 0;
    int rc = 0;
    if (!ctx || !resource_requests || ctx->planners.size () != len) {
        errno = EINVAL;
        return -1;
    }
    for (i = 0; i < ctx->planners.size (); ++i) {
        rc = planner_avail_during (ctx->planners[i], at, duration,
                                   resource_requests[i]);
        if (rc == -1)
            break;
    }
    return rc;
}

extern "C" int planner_multi_avail_resources_array_during (
                   planner_multi_t *ctx, int64_t at,
                   uint64_t duration, int64_t *resource_counts, size_t len)
{
    size_t i = 0;
    int64_t rc = 0;
    if (!ctx || !resource_counts
        || ctx->planners.size () < 1 || ctx->planners.size () != len) {
        errno = EINVAL;
        return -1;
    }
    for (i = 0; i < ctx->planners.size (); ++i) {
        rc = planner_avail_resources_during (ctx->planners[i], at, duration);
        if (rc == -1)
            break;
        resource_counts[i] = rc;
    }
    return (rc == -1)? -1 : 0;
}

extern "C" int64_t planner_multi_add_span (
                       planner_multi_t *ctx, int64_t start_time,
                       uint64_t duration,
                       const uint64_t *resource_requests, size_t len)
{
    size_t i = 0;
    int64_t span = -1;
    int64_t mspan = -1;

    if (!ctx || !resource_requests || len != ctx->planners.size ())
        return -1;

    mspan = ctx->span_counter;
    auto res = ctx->span_lookup.insert (
                        std::pair<int64_t, std::vector<int64_t>> (
                            mspan, std::vector<int64_t> ()));
    if (!res.second) {
        errno = EEXIST;
        return -1;
    }

    ctx->span_counter++;

    for (i = 0; i < len; ++i) {
        if ( (span = planner_add_span (ctx->planners[i],
                                       start_time, duration,
                                       resource_requests[i])) == -1) {
            ctx->span_lookup.erase (mspan);
            return -1;
        }
        ctx->span_lookup[mspan].push_back (span);
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
    auto it = ctx->span_lookup.find (span_id);
    if (it == ctx->span_lookup.end ()) {
        errno = ENOENT;
        goto done;
    }
    for (i = 0; i < it->second.size (); ++i) {
        if (planner_rem_span (ctx->planners[i], it->second[i]) == -1)
            goto done;
    }
    ctx->span_lookup.erase (it);
    rc  = 0;
done:
    return rc;
}

int64_t planner_multi_span_first (planner_multi_t *ctx)
{
    int64_t rc = -1;
    if (!ctx) {
        errno = EINVAL;
        goto done;
    }
    ctx->span_lookup_iter = ctx->span_lookup.begin ();
    if (ctx->span_lookup_iter == ctx->span_lookup.end ()) {
        errno = ENOENT;
        goto done;

    }
    rc = ctx->span_lookup_iter->first;
done:
    return rc;
}

extern "C" int64_t planner_multi_span_next (planner_multi_t *ctx)
{
    int64_t rc = -1;
    void *span = nullptr;
    if (!ctx) {
        errno = EINVAL;
        goto done;
    }
    ctx->span_lookup_iter++;
    if (ctx->span_lookup_iter == ctx->span_lookup.end ()) {
        errno = ENOENT;
        goto done;

    }
    rc = ctx->span_lookup_iter->first;
done:
    return rc;
}

extern "C" size_t planner_multi_span_size (planner_multi_t *ctx)
{
   if (!ctx) {
        errno = EINVAL;
        return 0;
    }
    return ctx->span_lookup.size ();
}

/*
 * vi: ts=4 sw=4 expandtab
 */
