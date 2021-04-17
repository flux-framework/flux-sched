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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <czmq.h>

#include "src/common/librbtree/rbtree.h"
#include "src/common/librbtree/rbtree_augmented.h"
#include "src/common/libutil/xzmalloc.h"
#include "src/common/libczmqcontainers/czmq_containers.h"
#include "planner.h"

#define START(node) ((node)->start)
#define LAST(node)  ((node)->last)

typedef struct span span_t;

typedef struct request {
    int64_t on_or_after;
    uint64_t duration;
    int64_t count;
} request_t;

/*! Scheduled point: time at which resource state changes.  Each point's resource
 *  requirements are tracked as a node in a min-time resource (MTR) binary search
 *  tree.
 */
typedef struct scheduled_point {
    struct rb_node point_rb;     /* BST node for scheduled point tree */
    struct rb_node resource_rb;  /* BST node for min-time resource tree */
    int64_t subtree_min;         /* Min time of the subtree of this node */
    int64_t at;                  /* Resource-state changing time */
    int in_mt_resource_tree;     /* 1 when inserted in min-time resource tree */
    int new_point;               /* 1 when this point is newly created */
    int ref_count;               /* reference counter */
    int64_t scheduled;           /* scheduled quantity at this point */
    int64_t remaining;           /* remaining resources (available) */
} scheduled_point_t;

/*! Node in a span interval tree to enable fast retrieval of intercepting spans.
 */
struct span {
    int64_t start;               /* start time of the span */
    int64_t last;                /* end time of the span */
    int64_t span_id;             /* unique span id */
    int64_t planned;             /* required resource quantity */
    int in_system;               /* 1 when inserted into the system */
    scheduled_point_t *start_p;  /* scheduled point object at start */
    scheduled_point_t *last_p;   /* scheduled point object at last */
};

/*! Planner context
 */
struct planner {
    int64_t total_resources;
    char *resource_type;
    int64_t plan_start;          /* base time of the planner */
    int64_t plan_end;            /* end time of the planner */
    struct rb_root sched_point_tree;  /* scheduled point rb tree */
    struct rb_root mt_resource_tree;  /* min-time resrouce rb tree */
    scheduled_point_t *p0;       /* system's scheduled point at base time */
    zhashx_t *span_lookup;       /* span lookup table by string id */
    zhashx_t *avail_time_iter;    /* tracking nodes temporarily deleted from MTR */
    request_t *current_request;  /* the req copy for avail time iteration */
    int avail_time_iter_set;     /* iterator set flag */
    uint64_t span_counter;       /* current span counter */
};


/*******************************************************************************
 *                                                                             *
 *                           INTERNAL PLANNER API                              *
 *                                                                             *
 *******************************************************************************/

/*******************************************************************************
 *                                                                             *
 *    Scheduled Points Binary Search Tree: O(log n) Scheduled Points Search    *
 *                                                                             *
 *******************************************************************************/
static scheduled_point_t *scheduled_point_search (int64_t t, struct rb_root *root)
{
    struct rb_node *node = root->rb_node;
    while (node) {
        scheduled_point_t *this_data = NULL;
        this_data = container_of (node, scheduled_point_t, point_rb);
        int64_t result = t - this_data->at;
        if (result < 0)
            node = node->rb_left;
        else if (result > 0)
            node = node->rb_right;
        else
            return this_data;
    }
    return NULL;
}

static inline scheduled_point_t *recent_state (scheduled_point_t *new_data,
                                               scheduled_point_t *old_data)
{
    if (!old_data)
        return new_data;
    return (new_data->at > old_data->at)? new_data : old_data;
}

/*! While scheduled_point_search returns the exact match scheduled_point_state
 *  returns the most recent scheduled point, representing the accurate resource
 *  state at the time t.
 */
static scheduled_point_t *scheduled_point_state (int64_t at, struct rb_root *root)
{
    scheduled_point_t *last_state = NULL;
    struct rb_node *node = root->rb_node;
    while (node) {
        scheduled_point_t *this_data = NULL;
        this_data = container_of (node, scheduled_point_t, point_rb);
        int64_t result = at - this_data->at;
        if (result < 0) {
            node = node->rb_left;
        } else if (result > 0) {
            last_state = recent_state (this_data, last_state);
            node = node->rb_right;
        } else {
            return this_data;
        }
    }
    return last_state;
}

static int scheduled_point_insert (scheduled_point_t *new_data,
                                   struct rb_root *root)
{
    struct rb_node **link = &(root->rb_node);
    struct rb_node *parent = NULL;
    while (*link) {
        scheduled_point_t *this_data = NULL;
        this_data  = container_of (*link, scheduled_point_t, point_rb);
        int64_t result = new_data->at - this_data->at;
        parent = *link;
        if (result < 0)
            link = &((*link)->rb_left);
        else if (result > 0)
            link = &((*link)->rb_right);
        else
            return -1;
    }
    rb_link_node (&(new_data->point_rb), parent, link);
    rb_insert_color (&(new_data->point_rb), root);
    return 0;
}

static int scheduled_point_remove (scheduled_point_t *data, struct rb_root *root)
{
    int rc = -1;
    scheduled_point_t *n = scheduled_point_search (data->at, root);
    if (n) {
        rb_erase (&(n->point_rb), root);
        // Note: this must only remove the node from the scheduled point tree:
        // DO NOT free memory allocated to the node
        rc = 0;
    }
    return rc;
}

static void scheduled_points_destroy (struct rb_node *node)
{
    if (node->rb_left)
        scheduled_points_destroy (node->rb_left);
    if (node->rb_right)
        scheduled_points_destroy (node->rb_right);
    scheduled_point_t *data = container_of (node, scheduled_point_t, point_rb);
    free (data);
}


/*******************************************************************************
 *                                                                             *
 *   Minimum Time Resource Tree: O(log n) Earliest Schedulable Point Search    *
 *                                                                             *
 *******************************************************************************/
static int64_t mintime_resource_subtree_min (scheduled_point_t *point)
{
    int64_t min = point->at;
    scheduled_point_t *p = NULL;
    if (point->resource_rb.rb_left) {
        p = rb_entry (point->resource_rb.rb_left, scheduled_point_t, resource_rb);
        if (min > p->subtree_min)
            min = p->subtree_min;
    }
    if (point->resource_rb.rb_right) {
        p = rb_entry (point->resource_rb.rb_right, scheduled_point_t, resource_rb);
        if (min > p->subtree_min)
            min = p->subtree_min;
    }
    return min;
}

static void mintime_resource_propagate (struct rb_node *n, struct rb_node *stop)
{
    int64_t subtree_min;
    while (n != stop) {
        scheduled_point_t *point = rb_entry (n, scheduled_point_t, resource_rb);
        subtree_min = mintime_resource_subtree_min (point);
        if (point->subtree_min == subtree_min)
            break;
        point->subtree_min = subtree_min;
        n = rb_parent (&(point->resource_rb));
    }
}

static void mintime_resource_copy (struct rb_node *src, struct rb_node *dst)
{
    scheduled_point_t *o = rb_entry (src, scheduled_point_t, resource_rb);
    scheduled_point_t *n = rb_entry (dst, scheduled_point_t, resource_rb);
    n->subtree_min = o->subtree_min;
}

static void mintime_resource_rotate (struct rb_node *src, struct rb_node *dst)
{
    scheduled_point_t *o = rb_entry (src, scheduled_point_t, resource_rb);
    scheduled_point_t *n = rb_entry (dst, scheduled_point_t, resource_rb);
    n->subtree_min = o->subtree_min;
    o->subtree_min = mintime_resource_subtree_min (o);
}

static const struct rb_augment_callbacks mintime_resource_aug_cb = {
    mintime_resource_propagate, mintime_resource_copy, mintime_resource_rotate
};

static void mintime_resource_insert (scheduled_point_t *new_data,
                                     struct rb_root *root)
{
    struct rb_node **link = &(root->rb_node);
    scheduled_point_t *this_data = NULL;
    struct rb_node *parent = NULL;
    while (*link) {
        this_data = rb_entry (*link, scheduled_point_t, resource_rb);
        parent = *link;
        if (this_data->subtree_min > new_data->at)
            this_data->subtree_min = new_data->at;
        if (new_data->remaining < this_data->remaining)
            link = &(this_data->resource_rb.rb_left);
        else
            link = &(this_data->resource_rb.rb_right);
    }
    new_data->subtree_min = new_data->at;
    new_data->in_mt_resource_tree = 1;
    rb_link_node (&(new_data->resource_rb), parent, link);
    rb_insert_augmented (&(new_data->resource_rb), root,
                         &mintime_resource_aug_cb);
}

static void mintime_resource_remove (scheduled_point_t *data,
                                     struct rb_root *root)
{
    rb_erase_augmented (&data->resource_rb, root, &mintime_resource_aug_cb);
    data->in_mt_resource_tree = 0;
}

static int64_t right_branch_mintime (struct rb_node *n)
{
    int64_t min_time = INT64_MAX;
    struct rb_node *right = n->rb_right;
    if (right)
        min_time = rb_entry (right, scheduled_point_t, resource_rb)->subtree_min;

    scheduled_point_t *this_data = rb_entry (n, scheduled_point_t, resource_rb);
    return  (this_data->at < min_time)? this_data->at : min_time;
}

static scheduled_point_t *find_mintime_point (struct rb_node *anchor,
                                              int64_t min_time)
{
    if (!anchor)
        return NULL;

    scheduled_point_t *this_data = NULL;
    this_data = rb_entry (anchor, scheduled_point_t, resource_rb);
    if (this_data->at == min_time)
        return this_data;

    struct rb_node *node = anchor->rb_right;
    while (node) {
        this_data = rb_entry (node, scheduled_point_t, resource_rb);
        if (this_data->at == min_time)
            return this_data;
        if (node->rb_left
            && (rb_entry(node->rb_left, scheduled_point_t,
                        resource_rb)->subtree_min == min_time))
            node = node->rb_left;
        else
            node = node->rb_right;
    }

    // Error condition: when an anchor was found, there must be
    // a point that meets the requirements.
    errno = ENOTSUP;
    return NULL;
}

static int64_t find_mintime_anchor (int64_t request, struct rb_root *mtrt,
                                    struct rb_node **anchor_p)
{
    struct rb_node *node = mtrt->rb_node;
    int64_t min_time = INT64_MAX;
    int64_t right_min_time = INT64_MAX;
    while (node) {
        scheduled_point_t *this_data = NULL;
        this_data = rb_entry (node, scheduled_point_t, resource_rb);
        if (request <= this_data->remaining) {
            // visiting node satisfies the resource requirements. This means all
            // nodes at its subtree also satisfy the requirements. Thus,
            // right_min_time is the best min time.
            right_min_time = right_branch_mintime (node);
            if (right_min_time < min_time) {
                min_time = right_min_time;
                *anchor_p = node;
            }
            // next, we should search the left subtree for potentially better
            // then current min_time;
            node = node->rb_left;
        } else {
            // visiting node does not satisfy the resource requirements. This
            // means that nothing in its left branch will meet these requirements:
            // time to search the right subtree.
            node = node->rb_right;
        }
    }
    return min_time;
}

static scheduled_point_t *mintime_resource_mintime (int64_t request,
                                                    struct rb_root *mtrt)
{
    struct rb_node *anchor = NULL;
    int64_t min_time = find_mintime_anchor (request, mtrt, &anchor);
    return find_mintime_point (anchor, min_time);
}


/*******************************************************************************
 *                                                                             *
 *                  Scheduled Point and Resource Update APIs                   *
 *                                                                             *
 *******************************************************************************/
static int track_points (zhashx_t *tracker, scheduled_point_t *point)
{
    char key[32];
    sprintf (key, "%jd", (intmax_t)point->at);
    // caller will rely on the fact that rc == -1 when key already exists.
    // don't need to register free */
    return zhashx_insert (tracker, key, point);
}

static void restore_track_points (planner_t *ctx)
{
    scheduled_point_t *point = NULL;
    struct rb_root *root = &(ctx->mt_resource_tree);
    zlistx_t *keys = zhashx_keys (ctx->avail_time_iter);
    const char *k = NULL;
    for (k = zlistx_first (keys); k; k = zlistx_next (keys)) {
        point = zhashx_lookup (ctx->avail_time_iter, k);
        mintime_resource_insert (point, root);
        zhashx_delete (ctx->avail_time_iter, k);
    }
    zlistx_destroy (&keys);
}

static void update_mintime_resource_tree (planner_t *ctx, zlist_t *list)
{
    scheduled_point_t *point = NULL;
    struct rb_root *mtrt = &(ctx->mt_resource_tree);
    for (point = zlist_first (list); point; point = zlist_next (list)) {
        if (point->in_mt_resource_tree)
            mintime_resource_remove (point, mtrt);
        if (point->ref_count && !(point->in_mt_resource_tree))
            mintime_resource_insert (point, mtrt);
    }
}

static void copy_req (request_t *dest, int64_t on_or_after, uint64_t duration,
                      uint64_t resource_count)
{
    dest->on_or_after = on_or_after;
    dest->duration = duration;
    dest->count = (int64_t)resource_count;
}

static scheduled_point_t *get_or_new_point (planner_t *ctx, int64_t at)
{
    struct rb_root *spt = &(ctx->sched_point_tree);
    scheduled_point_t *point = NULL;
    if ( !(point = scheduled_point_search (at, spt))) {
        struct rb_root *mtrt = &(ctx->mt_resource_tree);
        scheduled_point_t *state = scheduled_point_state (at, spt);
        point = xzmalloc (sizeof (*point));
        point->at = at;
        point->in_mt_resource_tree = 0;
        point->new_point = 1;
        point->ref_count = 0;
        point->scheduled = state->scheduled;
        point->remaining = state->remaining;
        scheduled_point_insert (point, spt);
        mintime_resource_insert (point, mtrt);
    }
    return point;
}

static void fetch_overlap_points (planner_t *ctx, int64_t at, uint64_t duration,
                                  zlist_t *list)
{
    struct rb_root *spr = &(ctx->sched_point_tree);
    scheduled_point_t *point = scheduled_point_state (at, spr);
    while (point) {
        if (point->at >= (at + (int64_t)duration))
            break;
        else if (point->at >= at)
            zlist_append (list, (void *)point);
        struct rb_node *n = rb_next (&(point->point_rb));
        point = rb_entry (n, scheduled_point_t, point_rb);
    }
}

static int update_points_add_span (planner_t *ctx, zlist_t *list, span_t *span)
{
    int rc = 0;
    scheduled_point_t *point = NULL;
    for (point = zlist_first (list); point; point = zlist_next (list)) {
        point->scheduled += span->planned;
        point->remaining -= span->planned;
        if ( (point->scheduled > ctx->total_resources)
              || (point->remaining < 0)) {
            errno = ERANGE;
            rc = -1;
        }
    }
    return rc;
}

static int update_points_subtract_span (planner_t *ctx, zlist_t *list,
                                        span_t *span)
{
    int rc = 0;
    scheduled_point_t *point = NULL;
    for (point = zlist_first (list); point; point = zlist_next (list)) {
        point->scheduled -= span->planned;
        point->remaining += span->planned;
        if ( (point->scheduled < 0)
              || (point->remaining > ctx->total_resources)) {
            errno = ERANGE;
            rc = -1;
        }
    }
    return rc;
}

static bool span_ok (planner_t *ctx, scheduled_point_t *start_point,
                     uint64_t duration, int64_t request)
{
    bool ok = true;
    struct rb_root *mtrt = &(ctx->mt_resource_tree);
    scheduled_point_t *next_point = NULL;
    struct rb_node *n = &(start_point->point_rb);
    while ((next_point = rb_entry (n, scheduled_point_t, point_rb))) {
         if (next_point->at >= (start_point->at + (int64_t)duration)) {
             ok = true;
             break;
         } else if (request > next_point->remaining) {
             mintime_resource_remove (start_point, mtrt);
             track_points (ctx->avail_time_iter, start_point);
             ok = false;
             break;
         }
         n = rb_next (&(next_point->point_rb));
    }
    return ok;
}

static int64_t avail_at (planner_t *ctx, int64_t on_or_after, uint64_t duration,
                         int64_t request)
{
    int64_t at = -1;
    scheduled_point_t *start_point = NULL;
    struct rb_root *mt = &(ctx->mt_resource_tree);
    while ((start_point = mintime_resource_mintime (request, mt))) {
        at = start_point->at;
        if (at < on_or_after) {
            mintime_resource_remove (start_point, mt);
            track_points (ctx->avail_time_iter, start_point);
            at = -1;

        } else if (span_ok (ctx, start_point, duration, request)) {
            mintime_resource_remove (start_point, mt);
            track_points (ctx->avail_time_iter, start_point);
            if ((at + duration) > ctx->plan_end)
                at = -1;
            break;
        }
    }
    return at;
}

static bool avail_during (planner_t *ctx, int64_t at, uint64_t duration,
                          const int64_t request)
{
    bool ok = true;
    struct rb_root *spr = NULL;
    if ((at + duration) > ctx->plan_end) {
        errno = ERANGE;
        return -1;
    }

    spr = &(ctx->sched_point_tree);
    scheduled_point_t *point = scheduled_point_state (at, spr);
    while (point) {
        if (point->at >= (at + (int64_t)duration)) {
            ok = true;
            break;
        } else if (request > point->remaining) {
            ok = false;
            break;
        }
        struct rb_node *n = rb_next (&(point->point_rb));
        point = rb_entry (n, scheduled_point_t, point_rb);
    }
    return ok;
}

static scheduled_point_t *avail_resources_during (planner_t *ctx, int64_t at,
                                                  uint64_t duration)
{
    struct rb_root *spr = NULL;

    if ((at + duration) > ctx->plan_end) {
        errno = ERANGE;
        return NULL;
    }

    spr = &(ctx->sched_point_tree);
    scheduled_point_t *point = scheduled_point_state (at, spr);
    scheduled_point_t *min = point;
    while (point) {
        if (point->at >= (at + (int64_t)duration))
            break;
        else if (min->remaining > point->remaining)
          min = point;

        struct rb_node *n = rb_next (&(point->point_rb));
        point = rb_entry (n, scheduled_point_t, point_rb);
    }
    return min;
}


/*******************************************************************************
 *                                                                             *
 *                              Utilities                                      *
 *                                                                             *
 *******************************************************************************/

static void initialize (planner_t *ctx, int64_t base_time, uint64_t duration)
{
    ctx->plan_start = base_time;
    ctx->plan_end = base_time + (int64_t)duration;
    ctx->sched_point_tree = RB_ROOT;
    ctx->mt_resource_tree = RB_ROOT;
    ctx->p0 = xzmalloc (sizeof (*(ctx->p0)));
    ctx->p0->at = base_time;
    ctx->p0->ref_count = 1;
    ctx->p0->remaining = ctx->total_resources;
    scheduled_point_insert (ctx->p0, &(ctx->sched_point_tree));
    mintime_resource_insert (ctx->p0, &(ctx->mt_resource_tree));
    ctx->span_lookup = zhashx_new ();
    ctx->avail_time_iter = zhashx_new ();
    ctx->current_request = xzmalloc (sizeof (*(ctx->current_request)));
    ctx->avail_time_iter_set = 0;
    ctx->span_counter = 0;
}

static inline void erase (planner_t *ctx)
{
    struct rb_node *n = NULL;
    if (ctx->span_lookup)
        zhashx_purge (ctx->span_lookup);
    zhashx_destroy (&(ctx->span_lookup));

    if (ctx->avail_time_iter) {
        zhashx_destroy (&ctx->avail_time_iter);
        ctx->avail_time_iter = NULL;
    }
    if (ctx->current_request) {
        free (ctx->current_request);
        ctx->current_request = NULL;
    }
    if (ctx->p0 && ctx->p0->in_mt_resource_tree)
        mintime_resource_remove (ctx->p0, &(ctx->mt_resource_tree));
    if ((n = ctx->sched_point_tree.rb_node))
        scheduled_points_destroy (n);
}

static inline bool not_feasable (planner_t *ctx, int64_t start_time,
                                 uint64_t duration, int64_t request)
{
    bool rc = (start_time < ctx->plan_start) || (duration < 1)
              || ((start_time + duration - 1) > ctx->plan_end);
    return rc;
}

static int span_input_check (planner_t *ctx, int64_t start_time,
                             uint64_t duration, int64_t request)
{
    int rc = -1;
    if (!ctx || not_feasable (ctx, start_time, duration, request)) {
        errno = EINVAL;
        goto done;
    } else if (request > ctx->total_resources || request < 0) {
        errno = ERANGE;
        goto done;
    }
    rc = 0;
done:
    return rc;
}

static span_t *span_new (planner_t *ctx, int64_t start_time, uint64_t duration,
                         uint64_t request)
{
    char key[32];
    span_t *span = NULL;
    if (span_input_check (ctx, start_time, duration, (int64_t)request) == -1)
        goto done;

    span = xzmalloc (sizeof (*span));
    span->start = start_time;
    span->last = start_time + duration;
    ctx->span_counter++;
    span->span_id = ctx->span_counter;
    span->planned = request;
    span->in_system = 0;
    span->start_p = NULL;
    span->last_p = NULL;
    sprintf (key, "%jd", (intmax_t)span->span_id);
    zhashx_insert (ctx->span_lookup, key, span);
    zhashx_freefn (ctx->span_lookup, key, free);
done:
    return span;
}


/*******************************************************************************
 *                                                                             *
 *                           PUBLIC PLANNER API                                *
 *                                                                             *
 *******************************************************************************/

planner_t *planner_new (int64_t base_time, uint64_t duration,
                        uint64_t resource_totals, const char *resource_type)
{
    planner_t *ctx = NULL;

    if (duration < 1 || !resource_type) {
        errno = EINVAL;
        goto done;
    } else if (resource_totals > INT64_MAX) {
        errno = ERANGE;
        goto done;
    }

    ctx = xzmalloc (sizeof (*ctx));
    ctx->total_resources = (int64_t)resource_totals;
    ctx->resource_type = xstrdup (resource_type);
    initialize (ctx, base_time, duration);

done:
    return ctx;
}

int planner_reset (planner_t *ctx, int64_t base_time, uint64_t duration)
{
    if (!ctx || duration < 1) {
        errno = EINVAL;
        return -1;
    }

    erase (ctx);
    initialize (ctx, base_time, duration);
    return 0;
}

void planner_destroy (planner_t **ctx_p)
{
    if (ctx_p && *ctx_p) {
        restore_track_points (*ctx_p);
        erase (*ctx_p);
        if ((*ctx_p)->resource_type)
            free ((*ctx_p)->resource_type);
        free (*ctx_p);
        *ctx_p = NULL;
    }
}

int64_t planner_base_time (planner_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    return ctx->plan_start;
}

int64_t planner_duration (planner_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    return ctx->plan_end - ctx->plan_start;
}

int64_t planner_resource_total (planner_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return -1;
    }
    return ctx->total_resources;
}

const char *planner_resource_type (planner_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return NULL;
    }
    return (const char *)ctx->resource_type;
}

int64_t planner_avail_time_first (planner_t *ctx, int64_t on_or_after,
                                  uint64_t duration, uint64_t request)
{
    int64_t t = -1;
    if (!ctx || on_or_after < ctx->plan_start
        || on_or_after >= ctx->plan_end || duration < 1) {
        errno = EINVAL;
        return -1;
    }
    if (request > ctx->total_resources) {
        errno = ERANGE;
        return -1;
    }
    restore_track_points (ctx);
    ctx->avail_time_iter_set = 1;
    copy_req (ctx->current_request, on_or_after, duration, request);
    if ( (t = avail_at (ctx, on_or_after, duration, (int64_t)request)) == -1)
        errno = ENOENT;
    return t;
}

int64_t planner_avail_time_next (planner_t *ctx)
{
    int64_t t = -1;
    int64_t on_or_after = -1;
    uint64_t duration = 0;
    int64_t request_count = 0;
    if (!ctx || !ctx->avail_time_iter_set) {
        errno = EINVAL;
        return -1;
    }
    request_count = ctx->current_request->count;
    on_or_after = ctx->current_request->on_or_after;
    duration = ctx->current_request->duration;
    if (request_count > ctx->total_resources) {
        errno = ERANGE;
        return -1;
    }
    if ( (t = avail_at (ctx, on_or_after, duration, (int64_t)request_count)) ==
          -1)
        errno = ENOENT;
    return t;
}

int planner_avail_during (planner_t *ctx, int64_t start_time, uint64_t duration,
                          uint64_t request)
{
    bool ok = false;
    if (!ctx || duration < 1) {
        errno = EINVAL;
        return -1;
    }
    if (request > ctx->total_resources) {
        errno = ERANGE;
        return -1;
    }
    ok = avail_during (ctx, start_time, duration, (int64_t)request);
    return ok? 0 : -1;
}

int64_t planner_avail_resources_during (planner_t *ctx, int64_t at,
                                        uint64_t duration)
{
    scheduled_point_t *min_point = NULL;
    if (!ctx || at > ctx->plan_end || duration < 1) {
        errno = EINVAL;
        return -1;
    }
    min_point = avail_resources_during (ctx, at, duration);
    return min_point->remaining;
}

int64_t planner_avail_resources_at (planner_t *ctx, int64_t at)
{
    struct rb_root *spt = NULL;
    scheduled_point_t *state = NULL;
    if (!ctx || at > ctx->plan_end) {
        errno = EINVAL;
        return -1;
    }
    spt = &(ctx->sched_point_tree);
    state = scheduled_point_state (at, spt);
    return state->remaining;
}

int64_t planner_add_span (planner_t *ctx, int64_t start_time,
                          uint64_t duration, uint64_t request)
{
    span_t *span = NULL;
    zlist_t *list = NULL;
    scheduled_point_t *start_point = NULL;
    scheduled_point_t *last_point = NULL;

    if (!avail_during (ctx, start_time, duration, (int64_t)request)) {
        errno = EINVAL;
        return -1;
    }
    if ( !(span = span_new (ctx, start_time, duration, request)))
        return -1;

    restore_track_points (ctx);
    list = zlist_new ();
    start_point = get_or_new_point (ctx, span->start);
    start_point->ref_count++;
    last_point = get_or_new_point (ctx, span->last);
    last_point->ref_count++;

    fetch_overlap_points (ctx, span->start, duration, list);
    update_points_add_span (ctx, list, span);

    start_point->new_point = 0;
    span->start_p = start_point;
    last_point->new_point = 0;
    span->last_p = last_point;

    update_mintime_resource_tree (ctx, list);

    zlist_destroy (&list);
    span->in_system = 1;
    ctx->avail_time_iter_set = 0;

    return span->span_id;
}

int planner_rem_span (planner_t *ctx, int64_t span_id)
{
    char key[32];
    int rc = -1;
    span_t *span = NULL;
    zlist_t *list = NULL;
    uint64_t duration = 0;

    if (!ctx) {
        errno = EINVAL;
        goto done;
    }
    sprintf (key, "%ju", (intmax_t)span_id);
    if ( !(span = zhashx_lookup (ctx->span_lookup, key))) {
        errno = EINVAL;
        goto done;
    }

    restore_track_points (ctx);
    list = zlist_new ();
    duration = span->last - span->start;
    span->start_p->ref_count--;
    span->last_p->ref_count--;
    fetch_overlap_points (ctx, span->start, duration, list);
    update_points_subtract_span (ctx, list, span);
    update_mintime_resource_tree (ctx, list);
    span->in_system = 0;

    if (span->start_p->ref_count == 0) {
        struct rb_root *mtrt = &(ctx->mt_resource_tree);
        scheduled_point_remove (span->start_p, &(ctx->sched_point_tree));
        if (span->start_p->in_mt_resource_tree)
            mintime_resource_remove (span->start_p, mtrt);
        free (span->start_p);
        span->start_p = NULL;
    }
    if (span->last_p->ref_count == 0) {
        struct rb_root *mtrt = &(ctx->mt_resource_tree);
        scheduled_point_remove (span->last_p, &(ctx->sched_point_tree));
        if (span->last_p->in_mt_resource_tree)
            mintime_resource_remove (span->last_p, mtrt);
        free (span->last_p);
        span->last_p = NULL;
    }
    zhashx_delete (ctx->span_lookup, key);
    zlist_destroy (&list);
    ctx->avail_time_iter_set = 0;
    rc = 0;

done:
    return rc;
}

int64_t planner_span_first (planner_t *ctx)
{
    int64_t rc = -1;
    span_t *span = NULL;
    if (!ctx) {
        errno = EINVAL;
        goto done;
    }
    if ( !(span = zhashx_first (ctx->span_lookup))) {
        errno = EINVAL;
        goto done;

    }
    rc = span->span_id;
done:
    return rc;
}

int64_t planner_span_next (planner_t *ctx)
{
    int64_t rc = -1;
    span_t *span = NULL;
    if (!ctx) {
        errno = EINVAL;
        goto done;
    }
    if ( !(span = zhashx_next (ctx->span_lookup))) {
        errno = EINVAL;
        goto done;

    }
    rc = span->span_id;
done:
    return rc;
}

size_t planner_span_size (planner_t *ctx)
{
    if (!ctx) {
        errno = EINVAL;
        return 0;
    }
    return zhashx_size (ctx->span_lookup);
}


bool planner_is_active_span (planner_t *ctx, int64_t span_id)
{
    bool rc = false;
    char key[32];
    span_t *span = NULL;
    if (!ctx) {
        errno = EINVAL;
        goto done;
    }
    sprintf (key, "%ju", (intmax_t)span_id);
    if ( !(span = zhashx_lookup (ctx->span_lookup, key))) {
        errno = EINVAL;
        goto done;
    }
    rc = (span->in_system)? true : false;
done:
    return rc;
}

int64_t planner_span_start_time (planner_t *ctx, int64_t span_id)
{
    char key[32];
    int64_t rc = -1;
    span_t *span = NULL;
    if (!ctx) {
        errno = EINVAL;
        goto done;
    }
    sprintf (key, "%ju", (intmax_t)span_id);
    if ( !(span = zhashx_lookup (ctx->span_lookup, key))) {
        errno = EINVAL;
        goto done;
    }
    rc = span->start;
done:
    return rc;
}

int64_t planner_span_duration (planner_t *ctx, int64_t span_id)
{
    char key[32];
    int64_t rc = -1;
    span_t *span = NULL;
    if (!ctx) {
        errno = EINVAL;
        goto done;
    }
    sprintf (key, "%ju", (intmax_t)span_id);
    if ( !(span = zhashx_lookup (ctx->span_lookup, key))) {
        errno = EINVAL;
        goto done;
    }
    rc = span->last - span->start;
done:
    return rc;
}

int64_t planner_span_resource_count (planner_t *ctx, int64_t span_id)
{
    char key[32];
    int64_t rc = -1;
    span_t *span = NULL;
    if (!ctx) {
        errno = EINVAL;
        goto done;
    }
    sprintf (key, "%ju", (intmax_t)span_id);
    if ( !(span = zhashx_lookup (ctx->span_lookup, key))) {
        errno = EINVAL;
        goto done;
    }
    rc = span->planned;
done:
    return rc;
}

/*
 * vi: ts=4 sw=4 expandtab
 */
