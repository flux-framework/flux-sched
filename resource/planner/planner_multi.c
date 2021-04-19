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

#include <stdlib.h>
#include <string.h>
#include <czmq.h>
#include <errno.h>

#include "src/common/libutil/xzmalloc.h"
#include "src/common/libczmqcontainers/czmq_containers.h"
#include "planner_multi.h"

struct request {
    int64_t on_or_after;
    uint64_t duration;
    int64_t *counts;
};

struct planner_multi {
    planner_t **planners;
    uint64_t *resource_totals;
    char **resource_types;
    size_t size;
    struct request iter;
    zhashx_t *span_lookup;
    uint64_t span_counter;
};

void fill_iter_request (planner_multi_t *ctx, struct request *iter,
                        int64_t at, uint64_t duration,
                        const uint64_t *resources, size_t len)
{
    iter->on_or_after = at;
    iter->duration = duration;
    memcpy (iter->counts, resources, len * sizeof (*resources));
}

planner_multi_t *planner_multi_new (int64_t base_time, uint64_t duration,
                                    const uint64_t *resource_totals,
                                    const char **resource_types, size_t len)
{
    int i = 0;
    planner_multi_t *ctx = NULL;

    if (duration < 1 || !resource_totals || !resource_types) {
        errno = EINVAL;
        goto done;
    } else {
        for (i = 0; i < len; ++i) {
            if (resource_totals[i] > INT64_MAX) {
                errno = ERANGE;
                goto done;
            }
        }
    }

    ctx = xzmalloc (sizeof (*ctx));
    ctx->resource_totals = xzmalloc (len * sizeof (*(ctx->resource_totals)));
    ctx->resource_types = xzmalloc (len * sizeof (*(ctx->resource_types)));
    ctx->planners = xzmalloc (len * sizeof (*(ctx->planners)));
    ctx->size = len;
    ctx->iter.on_or_after = 0;
    ctx->iter.duration = 0;
    ctx->iter.counts = xzmalloc (len * sizeof (*(ctx->iter.counts)));
    for (i = 0; i < len; ++i) {
        ctx->resource_totals[i] = resource_totals[i];
        ctx->resource_types[i] = xstrdup (resource_types[i]);
        ctx->planners[i] = planner_new (base_time, duration,
                                        resource_totals[i], resource_types[i]);
    }
    ctx->span_lookup = zhashx_new ();
    ctx->span_counter = 0;
done:
    return ctx;
}

int64_t planner_multi_base_time (planner_multi_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    return planner_base_time (ctx->planners[0]);
}

int64_t planner_multi_duration (planner_multi_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    return planner_duration (ctx->planners[0]);
}

size_t planner_multi_resources_len (planner_multi_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return 0;
    }
    return ctx->size;
}

const char **planner_multi_resource_types (planner_multi_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return NULL;
    }
    return (const char **)ctx->resource_types;
}

const uint64_t *planner_multi_resource_totals (planner_multi_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return NULL;
    }
    return (const uint64_t *)ctx->resource_totals;
}

int64_t planner_multi_resource_total_at (planner_multi_t *ctx, unsigned int i)
{
    int64_t rc = -1;
    if (ctx) {
        if (i >= ctx->size) {
            errno = EINVAL;
            goto done;
        }
        rc = planner_resource_total (ctx->planners[i]);
    }
done:
    return rc;
}

int64_t planner_multi_resource_total_by_type (planner_multi_t *ctx,
                                              const char *resource_type)
{
    int i = 0;
    int64_t rc = -1;
    if (!ctx || !resource_type)
        goto done;
    for (i = 0; i < ctx->size; i++) {
        if (!strcmp (ctx->resource_types[i], resource_type)) {
            rc = planner_resource_total (ctx->planners[i]);
            break;
        }
    }
    if (i == ctx->size)
        errno = EINVAL;
done:
    return rc;
}

int planner_multi_reset (planner_multi_t *ctx, int64_t base_time, uint64_t duration)
{
    int i = 0;
    int rc = -1;
    if (!ctx || duration < 1) {
        errno = EINVAL;
        goto done;
    }

    for (i = 0; i < ctx->size; ++i)
        if (planner_reset (ctx->planners[i], base_time, duration) == -1)
            goto done;

    rc = 0;
done:
    return rc;
}

void planner_multi_destroy (planner_multi_t **ctx_p)
{
    int i = 0;
    if (ctx_p && *ctx_p) {
        for (i = 0; i < (*ctx_p)->size; ++i) {
            planner_destroy (&((*ctx_p)->planners[i]));
            free ((*ctx_p)->resource_types[i]);
        }
        free ((*ctx_p)->resource_totals);
        free ((*ctx_p)->resource_types);
        free ((*ctx_p)->iter.counts);
        free ((*ctx_p)->planners);
        zhashx_destroy (&((*ctx_p)->span_lookup));
        free (*ctx_p);
        *ctx_p = NULL;
    }
}

planner_t *planner_multi_planner_at (planner_multi_t *ctx, unsigned int i)
{
    planner_t *planner = NULL;
    if (!ctx || i >= ctx->size) {
        errno = EINVAL;
        goto done;
    }
    planner = ctx->planners[i];
done:
    return planner;
}

int64_t planner_multi_avail_time_first (planner_multi_t *ctx,
                                        int64_t on_or_after, uint64_t duration,
                                        const uint64_t *resource_requests,
                                        size_t len)
{
    int i = 0;
    int unmet = 0;
    int64_t t = -1;

    if (!ctx || !resource_requests || ctx->size < 1
         || ctx->size != len) {
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
        for (i = 1; i < ctx->size; ++i) {
            if ((unmet = planner_avail_during (ctx->planners[i],
                                               t, duration,
                                               resource_requests[i])) == -1)
                break;
        }
    } while (unmet && (t = planner_avail_time_next (ctx->planners[0])) != -1);

done:
    return t;
}

int64_t planner_multi_avail_time_next (planner_multi_t *ctx)
{
    int i = 0;
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
        for (i = 1; i < ctx->size; ++i) {
            if ((unmet = planner_avail_during (ctx->planners[i], t,
                                               ctx->iter.duration,
                                               ctx->iter.counts[i])) == -1)
                break;
        }
    } while (unmet);

done:
    return t;
}

int64_t planner_multi_avail_resources_at (planner_multi_t *ctx, int64_t at,
                                          unsigned int i)
{
    if (!ctx || i >= ctx->size) {
        errno = EINVAL;
        return -1;
    }
    return planner_avail_resources_at (ctx->planners[i], at);
}

int planner_multi_avail_resources_array_at (planner_multi_t *ctx, int64_t at,
                                            int64_t *resource_counts,
                                            size_t len)
{
    int i = 0;
    int64_t rc = 0;
    if (!ctx || !resource_counts || ctx->size != len) {
        errno = EINVAL;
        return -1;
    }
    for (i = 0; i < ctx->size; ++i) {
        rc = planner_avail_resources_at (ctx->planners[i], at);
        if (rc == -1)
            break;
        resource_counts[i] = rc;
    }
    return (rc == -1)? -1 : 0;
}

int planner_multi_avail_during (planner_multi_t *ctx, int64_t at, uint64_t duration,
                                const uint64_t *resource_requests, size_t len)
{
    int i = 0;
    int rc = 0;
    if (!ctx || !resource_requests || ctx->size != len) {
        errno = EINVAL;
        return -1;
    }
    for (i = 0; i < ctx->size; ++i) {
        rc = planner_avail_during (ctx->planners[i], at, duration,
                                   resource_requests[i]);
        if (rc == -1)
            break;
    }
    return rc;
}

int planner_multi_avail_resources_array_during (planner_multi_t *ctx, int64_t at,
                                                uint64_t duration,
                                                int64_t *resource_counts,
                                                size_t len)
{
    int i = 0;
    int64_t rc = 0;
    if (!ctx || !resource_counts || ctx->size < 1 || ctx->size != len) {
        errno = EINVAL;
        return -1;
    }
    for (i = 0; i < ctx->size; ++i) {
        rc = planner_avail_resources_during (ctx->planners[i], at, duration);
        if (rc == -1)
            break;
        resource_counts[i] = rc;
    }
    return (rc == -1)? -1 : 0;
}

static void zlist_free_wrap (void *o)
{
    zlist_t *list = (zlist_t *)o;
    if (list)
        zlist_destroy (&list);
}

int64_t planner_multi_add_span (planner_multi_t *ctx, int64_t start_time,
                                uint64_t duration,
                                const uint64_t *resource_requests,
                                size_t len)
{
    char key[32];
    int i = 0;
    zlist_t *list = NULL;
    int64_t span = -1;
    int64_t mspan = -1;

    if (!ctx || !resource_requests || len != ctx->size)
        return -1;

    list = zlist_new ();
    mspan = ctx->span_counter;
    ctx->span_counter++;
    sprintf (key, "%jd", (intmax_t)mspan);

    for (i = 0; i < len; ++i) {
        if ((span = planner_add_span (ctx->planners[i],
                                      start_time, duration,
                                      resource_requests[i])) == -1)
            goto error;

        zlist_append (list, (void *)(intptr_t)span);
    }

    zhashx_insert (ctx->span_lookup, key, list);
    zhashx_freefn (ctx->span_lookup, key, zlist_free_wrap);
    return mspan;

error:
    zlist_destroy (&list);
    return -1;
}

int planner_multi_rem_span (planner_multi_t *ctx, int64_t span_id)
{
    int i = 0;
    int rc = -1;
    char key[32];
    void *s = NULL;
    zlist_t *list = NULL;

    if (!ctx || span_id < 0) {
        errno = EINVAL;
        goto done;
    }

    sprintf (key, "%jd", (intmax_t)span_id);
    if (!(list = zhashx_lookup (ctx->span_lookup, key))) {
        errno = EINVAL;
        goto done;
    }

    for (i = 0, s = zlist_first (list); s; i++, s = zlist_next (list))
        if (planner_rem_span (ctx->planners[i], (intptr_t)s) == -1)
            goto done;

    zhashx_delete (ctx->span_lookup, key);
    rc  = 0;
done:
    return rc;
}

int64_t planner_multi_span_first (planner_multi_t *ctx)
{
    int64_t rc = -1;
    void *span = NULL;
    if (!ctx) {
        errno = EINVAL;
        goto done;
    }
    if ( !(span = zhashx_first (ctx->span_lookup))) {
        errno = ENOENT;
        goto done;

    }
    rc = (intptr_t)span;
done:
    return rc;
}

int64_t planner_multi_span_next (planner_multi_t *ctx)
{
    int64_t rc = -1;
    void *span = NULL;
    if (!ctx) {
        errno = EINVAL;
        goto done;
    }
    if ( !(span = zhashx_next (ctx->span_lookup))) {
        errno = ENOENT;
        goto done;
    }
    rc = (intptr_t)span;
done:
    return rc;
}

size_t planner_multi_span_size (planner_multi_t *ctx)
{
   if (!ctx) {
        errno = EINVAL;
        return 0;
    }
    return zhashx_size (ctx->span_lookup);
}

/*
 * vi: ts=4 sw=4 expandtab
 */
