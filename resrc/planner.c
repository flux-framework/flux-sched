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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <czmq.h>

#include "src/common/libutil/xzmalloc.h"
#include "src/common/libutil/rbtree.h"
#include "src/common/libutil/rbtree_augmented.h"
#include "src/common/libutil/interval_tree_generic.h"
#include "planner.h"

#define DEBUG_PLANNER 0
#define START(node) ((node)->start)
#define LAST(node)  ((node)->last)
#define FREE_NOREF_POINT(rsv) do { \
    if ((rsv)->start_p->ref_count == 0) { \
        free ((rsv)->start_p); \
        (rsv)->start_p = NULL; \
    } \
    if ((rsv)->last_p->ref_count == 0) { \
        free ((rsv)->last_p); \
        (rsv)->last_p = NULL; \
    } \
} while (false);

#define OUT_OF_RANGE(A,B,C) (((A)-(B)) >= (C)? 1: 0)

typedef int64_t resrc_vector_t[MAX_RESRC_DIM];
typedef char *  rtype_vector_t[MAX_RESRC_DIM];
typedef struct rb_root rb_root_t;
typedef struct rb_node rb_node_t;

/* Scheduled point: a time at which resource state changes.  Each point's resource
 * requirements are tracked as a node in a min-time resource (MTR) binary search
 * tree -- MAX_RESRC_DIM dimensional.
 */
typedef struct scheduled_point {
    rb_node_t point_rb;       /* BST node for scheduled point search */
    rb_node_t resrc_rb;       /* Min time resource BST node */
    int64_t __subtree_min;    /* Min time of the subtree of this node */
    int64_t at;               /* Resource-state changing time */
    int inserted_to_resrc;    /* 1 when this point is inserted in min-time tree */
    int new_point;            /* 1 when this point is newly created */
    int ref_count;            /* reference counter */
    resrc_vector_t scheduled_resrcs; /* scheduled resources at this point */
    resrc_vector_t remaining_resrcs; /* remaining resources (available) */
} scheduled_point_t;

/* Reservation: a node in a reservation tree (interval tree) to enable fast
 * retrieval of intercepting reservations.
 */
struct reservation {
    rb_node_t resv_rb;        /* RB node for reservation interval tree */
    int64_t start;            /* start time of the reservation */
    int64_t last;             /* end time of the reservation */
    int64_t __subtree_last;   /* maximum end time of my subtree */
    int64_t resv_id;          /* unique reservation id */
    resrc_vector_t reserved_resrcs; /* required resources */
    size_t resrc_dim;         /* vector size of required resources */
    int added;                /* added to the reservation interval tree */
    struct scheduled_point *start_p; /* scheduled point object at start */
    struct scheduled_point *last_p;  /* scheduled point object at last */
};

/* Planner context
 */
struct planner {
    resrc_vector_t total_resrc_vector; /* total resources avail for planning */
    rtype_vector_t resrc_type_vector;  /* array of resrc type strings */
    size_t resrc_dim;         /* size of the above vector */
    int64_t plan_start;       /* begin of the planning span */
    int64_t plan_end;         /* end of the planning span */
    zhash_t *avail_time_iter; /* tracking points temporarily deleted from MTR */
    req_t *avail_time_iter_req;/* the req copy for avail time iteration */
    int avail_time_iter_set;   /* iterator set */
    scheduled_point_t *p1;     /* system's scheduled point at t0*/
    zhashx_t *r_lookup;         /* reservation look up table */
    rb_root_t reservations_root;     /* resource interval tree */
    rb_root_t scheduled_points_root; /* scheduled points red black BST */
    rb_root_t scheduled_resrcs_root; /* minimum time resource BST */
};


/*******************************************************************************
 *                                                                             *
 *                           INTERNAL PLANNER API                              *
 *                                                                             *
 *******************************************************************************/

/*******************************************************************************
 *                  Scheduled Points Binary Search Tree                        *
 *                 Efficient Searching of Scheduled Points                     *
 *******************************************************************************/
static scheduled_point_t *scheduled_point_search (int64_t t, struct rb_root *root)
{
    rb_node_t *node = root->rb_node;
    while (node) {
        scheduled_point_t *this_data = NULL;
        this_data = container_of(node, scheduled_point_t, point_rb);
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

/* while scheduled_point_search returns the exact match
 * scheduled_point_state returns the most recent scheduled point
 * -- which represents the resource state at the time t.
 */
static scheduled_point_t *scheduled_point_state (int64_t t, struct rb_root *root)
{
    scheduled_point_t *last_state = NULL;
    rb_node_t *node = root->rb_node;
    while (node) {
        scheduled_point_t *this_data = NULL;
        this_data = container_of(node, scheduled_point_t, point_rb);
        int64_t result = t - this_data->at;
        if (result < 0)
            node = node->rb_left;
        else if (result > 0) {
            last_state = recent_state (this_data, last_state);
            node = node->rb_right;
        }
        else
            return this_data;
    }
    return last_state;
}

static int scheduled_point_insert (scheduled_point_t *new_data, rb_root_t *root)
{
    rb_node_t **link = &(root->rb_node);
    rb_node_t *parent = NULL;
    while (*link) {
        scheduled_point_t *this_data = NULL;
        this_data  = container_of(*link, scheduled_point_t, point_rb);
        int64_t result = new_data->at - this_data->at;
        parent = *link;
        if (result < 0)
            link = &((*link)->rb_left);
        else if (result > 0)
            link = &((*link)->rb_right);
        else
            return -1;
    }
    rb_link_node(&(new_data->point_rb), parent, link);
    rb_insert_color(&(new_data->point_rb), root);
    return 0;
}

static int scheduled_point_remove (scheduled_point_t *data, struct rb_root *root)
{
    int rc = -1;
    scheduled_point_t *n = scheduled_point_search (data->at, root);
    if (n) {
        rb_erase (&(n->point_rb), root);
        /* Note: this should only remove the node from the scheduled point BST
         * and does NOT free memory allocated to the node
         */
        rc = 0;
    }
    return rc;
}

static void scheduled_points_destroy (rb_node_t *node)
{
    if (node->rb_left)
        scheduled_points_destroy (node->rb_left);
    if (node->rb_right)
        scheduled_points_destroy (node->rb_right);
    scheduled_point_t *data = container_of(node, scheduled_point_t, point_rb);
    free (data);
}


/*******************************************************************************
 *                          Reservation Interval Tree                          *
 *                       Efficient Intersection Searching                      *
 *******************************************************************************/
INTERVAL_TREE_DEFINE(struct reservation, resv_rb, int64_t, __subtree_last,
           START, LAST,, reservation)

static void reservations_destroy(rb_node_t *node)
{
    if (node->rb_left)
        reservations_destroy (node->rb_left);
    if (node->rb_right)
        reservations_destroy (node->rb_right);
    reservation_t *rsv = container_of(node, reservation_t, resv_rb);
    free (rsv);
}


/*******************************************************************************
 *                     Min Time Resource Tree                                  *
 *              Efficient Searching of Earliest Schedulable Points             *
 *******************************************************************************/
static inline int64_t scheduled_resrc_subtree_min (scheduled_point_t *point)
{
    int64_t min = point->at;
    int64_t subtree_min;
    if (point->resrc_rb.rb_left) {
        subtree_min = rb_entry(point->resrc_rb.rb_left,
                          scheduled_point_t, resrc_rb)->__subtree_min;
        if (min > subtree_min)
            min = subtree_min;
    }
    if (point->resrc_rb.rb_right) {
        subtree_min = rb_entry(point->resrc_rb.rb_right,
                          scheduled_point_t, resrc_rb)->__subtree_min;
        if (min > subtree_min)
            min = subtree_min;
    }
    return min;
}

static inline void scheduled_resrc_propagate (rb_node_t *rb, rb_node_t *stop)
{
    while (rb != stop) {
        scheduled_point_t *point = rb_entry(rb, scheduled_point_t, resrc_rb);
        int64_t subtree_min = scheduled_resrc_subtree_min (point);
        if (point->__subtree_min == subtree_min)
            break;
        point->__subtree_min = subtree_min;
        rb = rb_parent(&point->resrc_rb);
    }
}

static inline void scheduled_resrc_copy (rb_node_t *rb_old, rb_node_t *rb_new)
{
    scheduled_point_t *o = rb_entry(rb_old, scheduled_point_t, resrc_rb);
    scheduled_point_t *n = rb_entry(rb_new, scheduled_point_t, resrc_rb);
    n->__subtree_min = o->__subtree_min;
}

static inline void scheduled_resrc_rotate (rb_node_t *rb_old, rb_node_t *rb_new)
{
    scheduled_point_t *o = rb_entry(rb_old, scheduled_point_t, resrc_rb);
    scheduled_point_t *n = rb_entry(rb_new, scheduled_point_t, resrc_rb);
    n->__subtree_min = o->__subtree_min;
    o->__subtree_min = scheduled_resrc_subtree_min (o);
}

static const struct rb_augment_callbacks scheduled_resrc_aug_cb = {
    scheduled_resrc_propagate, scheduled_resrc_copy, scheduled_resrc_rotate
};

static inline int64_t veccmp (resrc_vector_t s1, resrc_vector_t s2, size_t len)
{
    int i = 0;
    int less = 0;
    int64_t r = 0;
    for (i = 0; i < len; ++i) {
        if ((r = (int64_t)s1[i] - (int64_t)s2[i]) > 0)
            break;
        less += r;
    }
    return (r > 0)? r : less;
}

static void scheduled_resrc_insert (scheduled_point_t *new_data, rb_root_t *root)
{
    rb_node_t **link = &(root->rb_node);
    scheduled_point_t *this_data = NULL;
    rb_node_t *parent = NULL;
    while (*link) {
        this_data = rb_entry(*link, scheduled_point_t, resrc_rb);
        parent = *link;
        if (this_data->__subtree_min > new_data->at)
            this_data->__subtree_min = new_data->at;
        int64_t result = 0;
        if ((result = veccmp (new_data->remaining_resrcs,
                          this_data->remaining_resrcs, MAX_RESRC_DIM)) < 0)
            link = &(this_data->resrc_rb.rb_left);
        else
            link = &(this_data->resrc_rb.rb_right);
    }
    new_data->__subtree_min = new_data->at;
    new_data->inserted_to_resrc = 1;
    rb_link_node(&(new_data->resrc_rb), parent, link);
    rb_insert_augmented(&(new_data->resrc_rb), root, &scheduled_resrc_aug_cb);
}

static void scheduled_resrc_remove (scheduled_point_t *data, rb_root_t *root)
{
    rb_erase_augmented (&data->resrc_rb, root, &scheduled_resrc_aug_cb);
    data->inserted_to_resrc = 0;
}

static inline int64_t rbranch_mintm (rb_node_t *node)
{
    int64_t mn = INT64_MAX;
    rb_node_t *r = node->rb_right;
    mn = r? rb_entry(r, scheduled_point_t, resrc_rb)->__subtree_min : mn;
    scheduled_point_t *this_data = rb_entry(node, scheduled_point_t, resrc_rb);
    return  (this_data->at < mn)? this_data->at : mn;
}

static inline scheduled_point_t *find_mintm_point (rb_node_t *anchor,
                                      int64_t mintm)
{
    if (!anchor)
        return NULL;

    scheduled_point_t *this_data = NULL;
    this_data = rb_entry(anchor, scheduled_point_t, resrc_rb);
    if (this_data->at == mintm)
        return this_data;

    rb_node_t *node = anchor->rb_right;
    while (node) {
        this_data = rb_entry(node, scheduled_point_t, resrc_rb);
        if (this_data->at == mintm)
            return this_data;

        if (node->rb_left
            && rb_entry(node->rb_left, scheduled_point_t,
                   resrc_rb)->__subtree_min == mintm)
            node = node->rb_left;
        else
            node = node->rb_right;
    }

    /* this is an error condition: when an anchor was found, there must be
     * a point that meets the requirements.
     */
    errno = ENOTSUP;
    return NULL;
}

static inline int64_t find_mintm_anchor (int64_t *rv, rb_root_t *rt,
                          rb_node_t **anchor_p)
{
    rb_node_t *node = rt->rb_node;
    int64_t mintm = INT64_MAX;
    int64_t r_mintm = INT64_MAX;

    while (node) {
        scheduled_point_t *this_data = NULL;
        this_data = rb_entry(node, scheduled_point_t, resrc_rb);
        int64_t result = 0;
        result = veccmp (rv, this_data->remaining_resrcs, MAX_RESRC_DIM);
        if (result <= 0) {
            /* visiting node satisfies the resource requirements this means all
             * of the nodes at its subtree also satisfies the requirements. Thus,
             * rbranch_mintime is the best min time.
             */
            r_mintm = rbranch_mintm (node);
            if (r_mintm < mintm) {
                mintm = r_mintm;
                *anchor_p = node;
            }
            /* next, we should search the left subtree for potentially better
             * then current mintm;
             */
            node = node->rb_left;
        } else {
            /* visiting node does not satisfy the resource requirements. This
             * means, nothing in its left branch will meet these requirements:
             * time to search the right subtree.
             */
            node = node->rb_right;
        }
    }
    return mintm;
}

static scheduled_point_t *scheduled_resrc_mintm (int64_t *rv, rb_root_t *rt)
{
    rb_node_t *anchor = NULL;
    int64_t mintm = find_mintm_anchor (rv, rt, &anchor);
    return find_mintm_point (anchor, mintm);
}

#if DEBUG_PLANNER
static void scheduled_resrc_print (rb_root_t *rt)
{
    rb_node_t *node;
    int i = 0;
    for (node = rb_first(rt); node; node = rb_next(node)) {
        i++;
        printf("..\n");
        printf("+ at=%ld\n",
            rb_entry(node, scheduled_point_t, resrc_rb)->at);
        printf("+ __subtree_min=%ld\n",
            rb_entry(node, scheduled_point_t, resrc_rb)->__subtree_min);
        printf("+ inserted_to_resrc=%d\n",
            rb_entry(node, scheduled_point_t, resrc_rb)->inserted_to_resrc);
        printf("+ new_point=%d\n",
            rb_entry(node, scheduled_point_t, resrc_rb)->new_point);
        printf("+ ref_count=%d\n",
            rb_entry(node, scheduled_point_t, resrc_rb)->ref_count);
        printf("+ scheduled_resrcs[0]=%jd\n", (intmax_t) rb_entry(node,
            scheduled_point_t, resrc_rb)->scheduled_resrcs[0]);
        printf("+ remaining_resrcs[0]=%ld\n", (intmax_t) rb_entry(node,
             scheduled_point_t, resrc_rb)->remaining_resrcs[0]);
    }
   printf ("SIZE: %d\n", i);
   printf ("===============================================================+=\n");
}
#endif


/*******************************************************************************
 *                  Scheduled Point and Resrc Update APIs                      *
 *                                                                             *
 *******************************************************************************/
static inline int track_points (zhash_t *tracker, struct scheduled_point *point)
{
    /* XXX OPTIMIZATION: Keep track of tracking status to avoid insert */
    /* XXX Use zlist or a new search tree */
    char key[32];
    sprintf (key, "%jd", (intmax_t)point->at);
    /* caller will rely on the fact that rc == -1 when key already exists */
    /* don't need to register free */
    return zhash_insert (tracker, key, point);
}

static inline void restore_track_points (planner_t *ctx, rb_root_t *root)
{
    scheduled_point_t *point = NULL;
    zlist_t *keys = zhash_keys (ctx->avail_time_iter);
    const char *k = NULL;
    for (k = zlist_first (keys); k; k = zlist_next (keys)) {
        point = zhash_lookup (ctx->avail_time_iter, k);
        scheduled_resrc_insert (point, root);
        zhash_delete (ctx->avail_time_iter, k);
    }
    zlist_destroy (&keys);
}

static inline int update_scheduled_resrcs (zhash_t *tracker, rb_root_t *rt)
{
    int rc = 0;
    const char *k = NULL;
    scheduled_point_t *point = NULL;
    zlist_t *keys = zhash_keys (tracker);
    for (k = zlist_first (keys); k; k = zlist_next (keys)) {
        point = zhash_lookup (tracker, k);
        if (point->inserted_to_resrc)
            scheduled_resrc_remove (point, rt);
        if (point->ref_count && !(point->inserted_to_resrc))
            scheduled_resrc_insert (point, rt);
        zhash_delete (tracker, k);
    }
    zlist_destroy (&keys);
    return rc;
}

static inline scheduled_point_t *add_P (planner_t *ctx, int64_t at,
                                     reservation_t *rsv, bool up)
{
    int i = 0;
    rb_root_t *rt = &(ctx->scheduled_points_root);
    scheduled_point_t *point = NULL;
    if (!(point = scheduled_point_search (at, rt))) {
        /* If point is not found, we must create a new scheduled point obj */
        point = xzmalloc (sizeof (*point));
        point->at = at;
        memset (point->scheduled_resrcs, '\0', sizeof (point->scheduled_resrcs));
        memcpy (point->remaining_resrcs, ctx->total_resrc_vector,
                sizeof (point->remaining_resrcs));
        point->inserted_to_resrc = 0; /* not been inserted to resource BST */
        point->new_point = 1;
        point->ref_count = 0;
        if (scheduled_point_insert (point, rt) < 0) {
            /* same key is rejected (should never happen) */
            errno = EKEYREJECTED;
            free (point);
            point = NULL;
            goto done;
        }
    }

    for (i = 0; up && i < rsv->resrc_dim; ++i) {
        point->scheduled_resrcs[i] += rsv->reserved_resrcs[i];
        point->remaining_resrcs[i] -= rsv->reserved_resrcs[i];
        if (point->scheduled_resrcs[i] > ctx->total_resrc_vector[i]
            || point->remaining_resrcs[i] < 0)
            errno = ERANGE;
    }

done:
    return point;
}

static inline int add_R (planner_t *ctx, reservation_t *rsv, zhash_t *tracker)
{
    rsv->start_p = add_P (ctx, rsv->start, rsv, true);
    rsv->last_p = add_P (ctx, rsv->last, rsv, false);
    if (rsv->start_p) {
        rsv->start_p->ref_count++;
        track_points (tracker, rsv->start_p);
    }
    if (rsv->last_p) {
        rsv->last_p->ref_count++;
        track_points (tracker, rsv->last_p);
    }
    return (!rsv->start_p || !rsv->last_p)? -1 : 0;
}

static inline int sub_R (planner_t *ctx, reservation_t *rsv, zhash_t *tracker)
{
    int rc = 0;
    int i = 0;

    if (rsv->start_p) {
        rsv->start_p->ref_count--;
        track_points (tracker, rsv->start_p);
        for (i = 0; i < rsv->resrc_dim; ++i) {
            rsv->start_p->scheduled_resrcs[i] -= rsv->reserved_resrcs[i];
            rsv->start_p->remaining_resrcs[i] += rsv->reserved_resrcs[i];
            if (rsv->start_p->scheduled_resrcs[i] < 0
                || rsv->start_p->remaining_resrcs[i] > ctx->total_resrc_vector[i]) {
                errno = ERANGE;
                rc = -1;
            }
        }
        if (!(rsv->start_p->ref_count))
            scheduled_point_remove (rsv->start_p, &(ctx->scheduled_points_root));
    }
    if (rsv->last_p) {
        rsv->last_p->ref_count--;
        track_points (tracker, rsv->last_p);
        if (!(rsv->last_p->ref_count))
            scheduled_point_remove (rsv->last_p, &(ctx->scheduled_points_root));
    }

    return (!rsv->start_p || !rsv->last_p)? -1 : rc;
}

static inline int add_I (planner_t *ctx, int64_t t, scheduled_point_t *p,
                      reservation_t *r, zhash_t *tracker, int force)
{
    int rc = 0;
    /* interception due to being equal has already been taken care */
    if ((START(r) < (t) && (t) < LAST(r))) {
        /* an existing point requires only one update w.r.t. new reservation
         * if a new point, it needs to be updated w.r.t. all existing ones (force)
         */
        if (track_points (tracker, p) == 0 || force) {
            int i = 0;
            for (i = 0; i < r->resrc_dim; ++i) {
                p->scheduled_resrcs[i] += r->reserved_resrcs[i];
                p->remaining_resrcs[i] -= r->reserved_resrcs[i];
                if (p->scheduled_resrcs[i] > ctx->total_resrc_vector[i]
                    || p->remaining_resrcs[i] < 0) {
                    rc = -1;
                    errno = ERANGE;
                }
            }
        }
    }
    return rc;
}

static inline int sub_I (planner_t *ctx, int64_t t, scheduled_point_t *p,
                      reservation_t *r, zhash_t *tracker)
{
    int rc = 0;
    /* interception due to being equal has already been taken care */
    if ((START(r) < (t) && (t) < LAST(r))) {
        /* an existing point requires only one update w.r.t. new reservation */
        if (track_points (tracker, p) == 0) {
            int i = 0;
            for (i = 0; i < r->resrc_dim; ++i) {
                p->scheduled_resrcs[i] -= r->reserved_resrcs[i];
                p->remaining_resrcs[i] += r->reserved_resrcs[i];
                if (p->scheduled_resrcs[i] > ctx->total_resrc_vector[i]
                    || p->remaining_resrcs[i] < 0) {
                    rc = -1;
                    errno = ERANGE;
                }
            }
        }
    }
    return rc;
}

static inline bool add_Is (planner_t *ctx, reservation_t *r1,
                       reservation_t *r2, zhash_t *tracker, int force)
{
    return ((add_I (ctx, START(r1), r1->start_p, r2, tracker,
                 force? r1->start_p->new_point : 0) == 0)
             && (add_I (ctx, LAST(r1), r1->last_p, r2, tracker,
                 force? r1->last_p->new_point : 0) == 0));

}

static inline bool sub_Is (planner_t *ctx, reservation_t *r1,
                       reservation_t *r2, zhash_t *tracker)
{
    return ((sub_I (ctx, START(r1), r1->start_p, r2, tracker) == 0)
             && (sub_I (ctx, LAST(r1), r1->last_p, r2, tracker) == 0));

}

static inline void copy_req (req_t *dest, req_t *src)
{
    dest->duration = src->duration;
    dest->vector_dim = src->vector_dim;
    size_t s1 = sizeof (*(dest->resrc_vector)) * MAX_RESRC_DIM;
    memset (dest->resrc_vector, '\0', s1);
    size_t s2 = sizeof (*(src->resrc_vector)) * src->vector_dim;
    memcpy (dest->resrc_vector, src->resrc_vector, s2);
}

static inline int64_t avail_time_internal (planner_t *ctx, req_t *req)
{
    int sat = 0;
    int64_t at = -1;
    int64_t *rv = NULL;
    int64_t *eff_rv = NULL;
    scheduled_point_t *p= NULL;
    rb_root_t *r = &(ctx->scheduled_resrcs_root);
    rv = (int64_t *)req->resrc_vector;

    if (veccmp (rv, ctx->total_resrc_vector, req->vector_dim) > 0) {
        errno = ERANGE;
        ctx->avail_time_iter_set = 0;
        goto done; /* unsatisfiable */
    }
    /* zero resource reservation is disallowed; a full resource check enough*/
    eff_rv = (req->exclusive)? ctx->total_resrc_vector : (int64_t *)rv;

    /* retrieve the minimum time when the requsted resources are available */
    while (!sat && (p = scheduled_resrc_mintm (eff_rv, r))) {
        rb_node_t *n = rb_next(&(p->point_rb));
        scheduled_point_t *d_chk = NULL;
        sat = 1;
        /* retrieve the next scheduled point and see if its time overlaps
         * with the request. If overlaps, check resource availability.
         */
        while ((d_chk = rb_entry(n, scheduled_point_t, point_rb))) {
            if (OUT_OF_RANGE(d_chk->at, p->at, req->duration))
                break;
            else {
                int64_t result;
                result = veccmp (eff_rv, d_chk->remaining_resrcs, req->vector_dim);
                if (result > 0) {
                    scheduled_resrc_remove (p, r);
                    track_points (ctx->avail_time_iter, p);
                    sat = 0;
                    break;
                }
            }
            n = rb_next (&(d_chk->point_rb));
        }
    }

   if (p) {
        at = p->at;
        scheduled_resrc_remove (p, r);
        track_points (ctx->avail_time_iter, p);
        if (!OUT_OF_RANGE(ctx->plan_end, at, req->duration))
            at = -1;
    }

done:
    return at;
}

static inline int avail_resources_at_internal (planner_t *ctx, int64_t starttime,
                      int64_t lasttime, int64_t *rv, int vd, int exclusive)
{
    int avail = -1;
    int64_t *eff_rv = NULL;
    if (starttime < 0 || !rv || !ctx) {
        errno = EINVAL;
        goto done;
    } else if (veccmp (rv, ctx->total_resrc_vector, vd) > 0) {
        errno = ERANGE;
        goto done;
    }

    eff_rv = exclusive? ctx->total_resrc_vector : (int64_t *)rv;
    rb_root_t *spr = &(ctx->scheduled_points_root);
    scheduled_point_t *state_at_start = NULL;

    if ((state_at_start = scheduled_point_state (starttime, spr)) == NULL) {
        errno = ENOTSUP;
        goto done;
    } else if (veccmp (eff_rv, state_at_start->remaining_resrcs, vd) > 0)
        goto done;

    rb_node_t *n = rb_next(&(state_at_start->point_rb));
    scheduled_point_t *d_chk = NULL;
    while ((d_chk = rb_entry(n, scheduled_point_t, point_rb))) {
        if (OUT_OF_RANGE(d_chk->at, starttime, (lasttime - starttime)))
            break;
        else {
            int64_t result;
            result = veccmp (eff_rv, d_chk->remaining_resrcs, vd);
            if (result > 0)
                goto done;
        }
        n = rb_next (&(d_chk->point_rb));
    }
    avail = 0;

done:
    return avail;
}


/*******************************************************************************
 *                              Utilities                                      *
 *                                                                             *
 *******************************************************************************/
static inline void planner_set_bound (planner_t *ctx, int64_t plan_starttime,
                       int64_t plan_duration)
{
    int i = 0;

    ctx->p1 = xzmalloc (sizeof (*(ctx->p1)));
    ctx->p1->at = plan_starttime;
    ctx->p1->ref_count = 1;
    memset (ctx->p1->scheduled_resrcs, '\0',
        sizeof (ctx->p1->scheduled_resrcs));
    memset (ctx->p1->remaining_resrcs, '\0',
        sizeof (ctx->p1->remaining_resrcs));
    for (i = 0; i < ctx->resrc_dim; ++i)
        ctx->p1->remaining_resrcs[i] = ctx->total_resrc_vector[i];
    ctx->plan_start = plan_starttime;
    ctx->plan_end = plan_starttime + plan_duration;
    ctx->avail_time_iter = zhash_new ();
    ctx->avail_time_iter_req = xzmalloc (sizeof (*(ctx->avail_time_iter_req)));
    size_t s = sizeof(*(ctx->avail_time_iter_req->resrc_vector)) * MAX_RESRC_DIM;
    ctx->avail_time_iter_req->resrc_vector = xzmalloc (s);
    ctx->avail_time_iter_set = 0;
    ctx->reservations_root = RB_ROOT;
    ctx->scheduled_points_root = RB_ROOT;
    ctx->scheduled_resrcs_root = RB_ROOT;
    scheduled_point_insert (ctx->p1, &(ctx->scheduled_points_root));
    scheduled_resrc_insert (ctx->p1, &(ctx->scheduled_resrcs_root));
}

static inline void planner_clean_internal (planner_t *ctx)
{
    if (ctx->avail_time_iter) {
        zhash_destroy (&ctx->avail_time_iter);
        ctx->avail_time_iter = NULL;
    }
    if (ctx->avail_time_iter_req) {
        if (ctx->avail_time_iter_req->resrc_vector)
            free (ctx->avail_time_iter_req->resrc_vector);
        free (ctx->avail_time_iter_req);
        ctx->avail_time_iter_req = NULL;
    }
    if (ctx->r_lookup)
        zhashx_purge (ctx->r_lookup);
    if (ctx->p1) {
        scheduled_resrc_remove (ctx->p1, &(ctx->scheduled_resrcs_root));
        ctx->p1 = NULL;
    }

    rb_node_t *n = NULL;
    if ((n = rb_first(&(ctx->scheduled_points_root))))
        scheduled_points_destroy (n);
    if ((n = rb_first(&(ctx->reservations_root))))
        reservations_destroy (n);
}

static inline bool not_feasable (planner_t *ctx, plan_t *plan)
{
    return (plan->start < ctx->plan_start || plan->req->duration < 1
            || plan->start + (plan->req->duration - 1) > ctx->plan_end
            || !plan->req->resrc_vector || plan->req->vector_dim > MAX_RESRC_DIM);
}

static inline int plan_input_check (planner_t *ctx, plan_t *plan)
{
    int i = 0;
    int rc = -1;
    char key[32];
    if (!ctx || !plan || !plan->req || not_feasable (ctx, plan)) {
        errno = EINVAL;
        goto done;
    } else {
        int64_t sum = 0;
        for (i = 0; i < plan->req->vector_dim; ++i) {
            if (plan->req->resrc_vector[i] > ctx->total_resrc_vector[i]) {
                errno = ERANGE;
                goto done;
            }
            sum += plan->req->resrc_vector[i];
        }
        if (sum <= 0) {
            errno = ERANGE;
            goto done;
       }
    }

    sprintf (key, "%jd", (intmax_t)plan->id);
    if (zhashx_lookup (ctx->r_lookup, key) != NULL) {
        errno = EINVAL;
        goto done;
    }
    rc = 0;

done:
    return rc;
}

static inline char *scheduled_point_to_string (scheduled_point_t *point)
{
    int i = 0;
    size_t size = 0;
    char *ptr = NULL;
    FILE *fptr = NULL;

    if (!point) {
        errno = EINVAL;
        goto done;
    } else if (!(fptr = open_memstream (&ptr, &size))) {
        errno = ENOMEM;
        goto done;
    }

    if (fprintf (fptr, "\t SCHEDULED POINT INFO\n") < 0)
        goto done;
    else if (fprintf (fptr, "\t\t at: %jd\n", (intmax_t)point->at) < 0)
        goto done;

    for (i = 0; i < MAX_RESRC_DIM; ++i) {
        if (fprintf (fptr, "\t\t scheduled resources for type %d: %ju\n", i,
                (intmax_t)point->scheduled_resrcs[i]) < 0)
            goto done;
        else if (fprintf (fptr, "\t\t remaining resources for type %d: %ju\n", i,
                (intmax_t)point->remaining_resrcs[i]) < 0)
            goto done;
    }

done:
    if (fptr)
        fclose (fptr);
    return ptr;
}

static inline int print_csv (planner_t *ctx, FILE *fptr, size_t d)
{
    rb_node_t *n = NULL;
    for (n = rb_first(&(ctx->scheduled_points_root)); n; n = rb_next(n)) {
        scheduled_point_t *data = container_of(n, scheduled_point_t, point_rb);
        if (fprintf (fptr, "%jd %jd\n", (intmax_t)data->at,
                     (intmax_t)data->scheduled_resrcs[d]) < 0)
            return -1;
    }
    return 0;
}

static inline int print_gp (planner_t *ctx, FILE *fptr,
                      const char *csvfn, size_t d)
{
    int rc = 0;
    if (!fptr || !csvfn || d > MAX_RESRC_DIM || !ctx) {
        errno = EINVAL;
        return -1;
    }

    rc = fprintf (fptr, "reset\n");
    rc += fprintf (fptr, "set terminal png size 1024 768\n");
    rc += fprintf (fptr, "set yrange [0:%jd]\n", (ctx->total_resrc_vector[d]+50));
    rc += fprintf (fptr, "set xlabel \"Scheduled Points in Time\"\n");
    rc += fprintf (fptr, "set ylabel \"Scheduled Resources of Type %d\"\n", (int)d);
    rc += fprintf (fptr, "set title \"Scheduled Resources Over Time\"\n");
    rc += fprintf (fptr, "set key below\n");
    rc += fprintf (fptr, "plot \"%s\" using 1:2 with steps lw 2 \n", csvfn);
    return rc;
}


/*******************************************************************************
 *                                                                             *
 *                           PUBLIC PLANNER API                                *
 *                                                                             *
 *******************************************************************************/

/*******************************************************************************
 *                              C'Tor/D'Tor                                    *
 *******************************************************************************/
planner_t *planner_new (int64_t plan_starttime, int64_t plan_duration,
                      uint64_t *total_resrcs, size_t len)
{
    int i = 0;
    planner_t *ctx = NULL;

    if (plan_starttime < 0 || plan_duration < 1
        || !total_resrcs || len > MAX_RESRC_DIM) {
        errno = EINVAL;
        goto done;
    } else {
        for (i = 0; i < len; ++i) {
            if (total_resrcs[i] > INT64_MAX) {
                errno = ERANGE;
                goto done;
            }
        }
    }

    ctx = xzmalloc (sizeof (*ctx));
    ctx->resrc_dim = len;
    ctx->r_lookup = zhashx_new ();
    memset (ctx->total_resrc_vector, '\0', sizeof (ctx->total_resrc_vector));
    for (i = 0; i < len; ++i)
        ctx->total_resrc_vector[i] = (int64_t)total_resrcs[i];
    for (i = 0; i < MAX_RESRC_DIM; ++i)
        ctx->resrc_type_vector[i] = NULL;
    planner_set_bound (ctx, plan_starttime, plan_duration);

done:
    return ctx;
}

void planner_destroy (planner_t **ctx_p)
{
    if (ctx_p && *ctx_p) {
        planner_clean_internal (*ctx_p);
        zhashx_destroy (&((*ctx_p)->r_lookup));
        free (*ctx_p);
        *ctx_p = NULL;
    }
}

int planner_reset (planner_t *ctx, int64_t plan_starttime, int64_t plan_duration,
        uint64_t *total_resrcs, size_t len)
{
    int i = 0;
    int rc = -1;
    if (plan_starttime < 0 || plan_duration < 1 || len > MAX_RESRC_DIM) {
        errno = EINVAL;
        goto done;
    } else if (total_resrcs && !len) {
        for (i = 0; i < len; ++i) {
            if (total_resrcs[i] > INT64_MAX) {
                errno = ERANGE;
                goto done;
            }
        }
    }

    planner_clean_internal (ctx);
    if (total_resrcs && !len) {
        memset (ctx->total_resrc_vector, '\0', sizeof (ctx->total_resrc_vector));
        for (i = 0; i < len; ++i)
            ctx->total_resrc_vector[i] = (int64_t)total_resrcs[i];
    }
    planner_set_bound (ctx, plan_starttime, plan_duration);
    rc = 0;

done:
    return rc = 0;
}

int64_t planner_plan_starttime (planner_t *ctx)
{
    return ctx? ctx->plan_start : -1;
}

int64_t planner_plan_duration (planner_t *ctx)
{
    return ctx? (ctx->plan_end - ctx->plan_start) : -1;
}

const uint64_t *planner_total_resrcs (planner_t *ctx)
{
    return ctx? (const uint64_t *)ctx->total_resrc_vector : NULL;
}

size_t planner_total_resrcs_len (planner_t *ctx)
{
    return ctx? ctx->resrc_dim : -1;
}

int planner_set_resrc_types (planner_t *ctx, const char **rts, size_t len)
{
    int i = 0, j = 0;

    if (rts == NULL || len > ctx->resrc_dim)
        return -1;

    for (i = 0; i < len; ++i) {
        if (ctx->resrc_type_vector[i] != NULL) {
            free (ctx->resrc_type_vector[i]);
            ctx->resrc_type_vector[i] = NULL;
        }
        ctx->resrc_type_vector[i] = xstrdup (rts[i]);
    }

    for (j = i; j < ctx->resrc_dim; ++j) {
        if (ctx->resrc_type_vector[i] != NULL) {
            free (ctx->resrc_type_vector[i]);
            ctx->resrc_type_vector[i] = NULL;
        }
    }

    return 0;
}

const char *planner_resrc_index2type (planner_t *ctx, int i)
{
    if (i < 0 || i >= ctx->resrc_dim)
        return NULL;
    return ctx->resrc_type_vector[i];
}

int planner_resrc_type2index (planner_t *ctx, const char *t)
{
    int i = 0;
    if (t == NULL)
        return -1;

    for (i = 0; i < ctx->resrc_dim; ++i) {
        if (strcmp (ctx->resrc_type_vector[i], t) == 0)
            break;
    }
    return (i < ctx->resrc_dim)? i : -1;
}

int64_t planner_avail_time_first (planner_t *ctx, req_t *req)
{
    if (!req || !ctx) {
        errno = EINVAL;
        return -1;
    }
    restore_track_points (ctx, &(ctx->scheduled_resrcs_root));
    copy_req (ctx->avail_time_iter_req, req);
    ctx->avail_time_iter_set = 1;
    return avail_time_internal (ctx, ctx->avail_time_iter_req);
}

int64_t planner_avail_time_next (planner_t *ctx)
{
    if (!ctx || !ctx->avail_time_iter_set) {
        errno = EINVAL;
        return -1;
    }
    return avail_time_internal (ctx, ctx->avail_time_iter_req);
}

int planner_avail_resources_at (planner_t *ctx, int64_t starttime, req_t *req)
{
    return avail_resources_at_internal (ctx, starttime, starttime + req->duration,
               (int64_t *)req->resrc_vector, req->vector_dim, req->exclusive);
}

reservation_t *planner_reservation_new (planner_t *ctx, plan_t *plan)
{
    int i = 0;
    reservation_t *rsv = NULL;
    char key[32];

    if (plan_input_check (ctx, plan) == -1)
        goto done;

    rsv = xzmalloc (sizeof (*rsv));
    rsv->start = plan->start;
    rsv->last = plan->start + plan->req->duration;
    rsv->resv_id = plan->id;
    memset (rsv->reserved_resrcs, '\0', sizeof (rsv->reserved_resrcs));
    rsv->resrc_dim = plan->req->vector_dim;
    for (i = 0; i < plan->req->vector_dim; ++i)
        rsv->reserved_resrcs[i] = (int64_t)plan->req->resrc_vector[i];
    rsv->added = 0;
    rsv->start_p = NULL;
    rsv->last_p = NULL;
    sprintf (key, "%jd", (intmax_t)rsv->resv_id);
    zhashx_insert (ctx->r_lookup, key, rsv);

done:
    return rsv;
}

void planner_reservation_destroy (planner_t *ctx, reservation_t **rsv_p)
{
    char key[32];
    if (!rsv_p || !(*rsv_p)) {
        errno = EINVAL;
        return;
    }
    sprintf (key, "%jd", (intmax_t)(*rsv_p)->resv_id);
    zhashx_delete (ctx->r_lookup, key);
    if ((*rsv_p)->added)
        planner_rem_reservation (ctx, (*rsv_p));

    free ((*rsv_p));
    *rsv_p = NULL;
}

int planner_add_reservation (planner_t *ctx, reservation_t *rsv, int validate)
{
    int rc = -1;
    if (!rsv || !ctx) {
        errno = EINVAL;
        goto done2;
    } else if (rsv->added) {
        goto done2;
    } else if (validate == 1) {
        if (avail_resources_at_internal (ctx, rsv->start,
                rsv->last, rsv->reserved_resrcs, rsv->resrc_dim, 0) == -1)
            goto done2;
    }

    rb_root_t *srr = &(ctx->scheduled_resrcs_root);
    rb_root_t *rr = &(ctx->reservations_root);
    restore_track_points (ctx, srr);

    /* tr is used to keep track of the scheduled points that
     * need to be updated in the min-time resource tree
     */
    zhash_t *tr = zhash_new ();

    /* update the specific start and last scheduled points
     * if a point already exist, simply update; otherwise
     * a new point object is inserted into scheduled point tree
     */
    if ((rc = add_R (ctx, rsv, tr)) < 0)
        goto done;

    /*
     * Go through all of the reservations that each of the two scheduled
     * points of the new reservation intersects and update relevant points
     */
    reservation_t *i = NULL;
    for (i = reservation_iter_first (rr, START(rsv), LAST(rsv)); i;
         i = reservation_iter_next (i, START(rsv), LAST(rsv))) {

        /* The point(s) of the intercepting reservation intersects the new one.
         * The point(s) of the new reservation intercept the old one.
         */
        if (!add_Is (ctx, i, rsv, tr, 0) || !add_Is (ctx, rsv, i, tr, 1))
            goto done;
    }
    rsv->start_p->new_point = 0;
    rsv->last_p->new_point = 0;

    /* Update the min-time resource tree w.r.t. tracked scheduled points */
    if ((rc = update_scheduled_resrcs (tr, srr)) < 0)
        goto done;

    reservation_insert (rsv, rr);
    rsv->added = 1;
    rc = 0;

done:
    if (tr)
        zhash_destroy (&tr);
done2:
    return rc;
}

int planner_rem_reservation (planner_t *ctx, reservation_t *rsv)
{
    int rc = -1;
    if (!rsv || !ctx) {
        errno = EINVAL;
        goto done2;
    } else if (rsv->added != 1)
        goto done2;

    rb_root_t *srr = &(ctx->scheduled_resrcs_root);
    rb_root_t *rr = &(ctx->reservations_root);
    reservation_t *i = NULL;
    restore_track_points (ctx, srr);

    /* tr is used to keep track of the scheduled points that
     * need to be updated in the min-time resource tree
     */
    zhash_t *tr = zhash_new ();

    /* update the specific start and last scheduled points
     * if a point already exist, simply update; otherwise
     * a new point object is inserted into scheduled point tree
     */
    if ((rc = sub_R (ctx, rsv, tr)) < 0)
        goto done;

    /*
     * Go through all of the reservations that each of the two scheduled
     * points of the new reservation intersects and update relevant points
     */
    for (i = reservation_iter_first (rr, START(rsv), LAST(rsv)); i;
         i = reservation_iter_next (i, START(rsv), LAST(rsv))) {
        if (!sub_Is (ctx, i, rsv, tr))
            goto done;
    }

    if ((rc = update_scheduled_resrcs (tr, srr)) < 0)
        goto done;

    reservation_remove (rsv, rr);
    FREE_NOREF_POINT(rsv);
    rsv->added = 0;
    rc = 0;

done:
    if (tr)
        zhash_destroy (&tr);
done2:
    return rc;
}

reservation_t *planner_reservation_first (planner_t *ctx)
{
    int64_t s = ctx->plan_start;
    int64_t e = ctx->plan_end;
    return reservation_iter_first (&(ctx->reservations_root), s, e);
}

reservation_t *planner_reservation_next (planner_t *ctx, reservation_t *rsv)
{
    return reservation_iter_next (rsv, ctx->plan_start, ctx->plan_end);
}

reservation_t *planner_reservation_by_id (planner_t *ctx, int64_t id)
{
    char key[32];
    sprintf (key, "%jd", (intmax_t)id);
    return zhashx_lookup (ctx->r_lookup, key);
}

reservation_t *planner_reservation_by_id_str (planner_t *ctx, const char *str)
{
    return (str)? zhashx_lookup (ctx->r_lookup, str) : NULL;
}

int planner_reservation_added (planner_t *ctx, reservation_t *rsv)
{
    if (!ctx || !rsv) {
        errno = EINVAL;
        return -1;
    }
    return rsv->added? 0 : -1;
}

int64_t planner_reservation_starttime (planner_t *ctx, reservation_t *rsv)
{
    if (!ctx || !rsv) {
        errno = EINVAL;
        return -1;
    }
    return rsv->start;
}

int64_t planner_reservation_endtime (planner_t *ctx, reservation_t *rsv)
{
    if (!ctx || !rsv) {
        errno = EINVAL;
        return -1;
    }
    return rsv->last;
}

const uint64_t *planner_reservation_reserved (planner_t *ctx, reservation_t *rsv,
                    size_t *len)
{
    if (!ctx || !rsv) {
        errno = EINVAL;
        return NULL;
    }
    *len = rsv->resrc_dim;
    return (const uint64_t *) rsv->reserved_resrcs;
}

char *planner_reservation_to_string (planner_t *ctx, reservation_t *rsv)
{
    int i = 0;
    size_t size = 0;
    char *ptr = NULL;
    FILE *fptr = NULL;

    if (!rsv) {
        errno = EINVAL;
        goto done;
    } else if (!(fptr = open_memstream (&ptr, &size))) {
        errno = ENOMEM;
        goto done;
    }

    if (fprintf (fptr, "Reservation Info:\n") < 0)
        goto done;
    else if (fprintf (fptr, "\t id: %jd\n", (intmax_t)rsv->resv_id) < 0)
        goto done;
    else if (fprintf (fptr, "\t start: %jd\n", (intmax_t)rsv->start) < 0)
        goto done;
    else if (fprintf (fptr, "\t last: %jd\n", (intmax_t)rsv->last) < 0)
        goto done;

    for (i = 0; i < rsv->resrc_dim; ++i) {
        if (fprintf (fptr, "    - reserved_resrcs type %d: %ju\n", i,
               (intmax_t)rsv->reserved_resrcs[i]) < 0)
            goto done;
    }

    if (fprintf (fptr, "%s", scheduled_point_to_string (rsv->start_p)) < 0)
        goto done;
    else if (fprintf (fptr, "%s", scheduled_point_to_string (rsv->last_p)) < 0)
        goto done;

done:
    if (fptr)
        fclose (fptr);
    return ptr;
}

int planner_print_gnuplot (planner_t *ctx, const char *fname, size_t d)
{
    int rc = -1;
    char *path1 = NULL;
    char *path2 = NULL;
    FILE *fptr1 = NULL;
    FILE *fptr2 = NULL;

    if (!fname || d > MAX_RESRC_DIM || !ctx) {
        errno = EINVAL;
        goto done;
    }

    if (!(path1 = xasprintf ("%s.csv", fname)))
        goto done;
    else if (!(path2 = xasprintf ("%s.gp", fname)))
        goto done;
    else if (!(fptr1 = fopen (path1, "w")))
        goto done;
    else if (!(fptr2 = fopen (path2, "w")))
        goto done;
    else if (print_csv (ctx, fptr1, d) < 0)
        goto done;
    else if (print_gp (ctx, fptr2, path1, d) < 0)
        goto done;

    rc = 0;

done:
    if (fptr1)
        fclose (fptr1);
    if (fptr2)
        fclose (fptr2);
    if (path1)
        free (path1);
    if (path2)
        free (path2);
    return rc;
}

/*
 * vi: ts=4 sw=4 expandtab
 */
