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

/* Planner provides a simple API and efficient mechanisms to allow
 * a Flux scheduler to keep track of the state of resource aggregates
 * of a composite resource.
 *
 * In a resource hierarchy used by flux-sched (e.g., hardware
 * hierarchy), a composite resource is represented as a tree graph
 * in which a higher-level vertex has essentially pointers to its
 * immediate child resources, each of which also has pointers to
 * its immediate children etc. With such an organization, the
 * scheduler must essentially walk "all" of the vertices below any
 * composite resource in order to determine if the "sub-resources"
 * requirement can be met.
 *
 * When the scheduler performs such walks excessively in particular,
 * on large graph, however, this can quickly become a performance and
 * scalability bottleneck. Planner addresses this problem by allowing
 * the scheduler to track the "sub-resources" summary information
 * (i.e., aggregates) efficiently at each upper-level composite
 * resource vertex and to use this aggregate information to prune
 * unneccessary descent down into the subtree.
 *
 * Planner offers update and query APIs to support these schemes.
 * Through a planner API, the scheduler can ask a high-level composite
 * a question: "given a request of x, y, z "sub-resources" in aggregate
 * for d time unit, when is the earliest time t at which this request
 * can be satisfied?"
 * Another example would be to answer, "from time t to t+d, does
 * this composite resource vertex has y, z sub-resources available
 * in aggregate.  By composing these queries at different levels in a
 * resource hierarchy, the scheduler can significantly reduce the
 * numbers of tree walks.  Ultimately, planner will be integrated
 * into our preorder tree-walk pruning filter in our future
 * visitor-pattern-based resource matching scheme.
 */

#ifndef PLANNER_H
#define PLANNER_H

#include <stddef.h>
#include <stdint.h>
#include <errno.h>

#define MAX_RESRC_DIM 5

typedef struct request {
    uint64_t duration;
    uint64_t *resrc_vector;
    size_t vector_dim;
    int exclusive;
} req_t;

typedef struct plan {
    int64_t id;
    int64_t start;
    struct request *req;
} plan_t;

typedef struct reservation reservation_t;
typedef struct planner planner_t;

/* Planner constructor:
 *
 *     - plan_starttime: the earliest schedulable point (in time)
 *            planned by this planner.
 *     - plan_duration: the span of this planner--i.e., all reservations
 *            must end before plan_starttime + plan_duration.
 *     - total_resrcs: an array of size of len containing
 *            total numbers of available resources (of up to
 *            five different types) used in this planner. Each
 *            element of this array would often represent the
 *            total number of each sub-resource under the target
 *            composite resource. Note that nothing prevents
 *            one from using this to represent the numbers
 *            or amounts of available resources directly at
 *            the resource vertex itself, though.
 *     - len: must be less than or equal to MAX_RESRC_DIM
 */
planner_t *planner_new (int64_t plan_starttime, int64_t plan_duration,
              uint64_t *total_resrcs, size_t len);

/* Reset the planner with new time bound and optionally resource quantities.
 * Destroy all of the existing reservations.
 *
 *     - plan_starttime: the earliest schedulable point (in time)
 *            planned by this planner.
 *     - plan_duration: the span of this planner--i.e., all reservations
 *            must end before plan_starttime + plan_duration.
 *     - total_resrcs: an array of size of len containing
 *            total numbers of available resources (of up to
 *            five different types) used in this planner. Each
 *            element of this array would often represent the
 *            total number of each sub-resource under the target
 *            composite resource. Note that nothing prevents
 *            one from using this to represent the numbers
 *            or amounts of available resources directly at
 *            the resource vertex itself, though.
 *            If NULL, the existing resource quantities will be used.
 *     - len: must be less than or equal to MAX_RESRC_DIM.
 *            pass 0, if the existing resource quantities must
 *            be used.
 */
int planner_reset (planner_t *ctx, int64_t plan_starttime, int64_t plan_duration,
        uint64_t *total_resrcs, size_t len);

/* Planner destructor:
 *
 *     - ctx_p: a pointer to the opaque planner context returned
 *            from planner_new.
 */
void planner_destroy (planner_t **ctx_p);

/* Getters:
 */
int64_t planner_plan_starttime (planner_t *ctx);
int64_t planner_plan_duration (planner_t *ctx);
const uint64_t *planner_total_resrcs (planner_t *ctx);
size_t planner_total_resrcs_len (planner_t *ctx);

/* Set resource type strings corresponding to resources planned by this
 * planner. rts is an array of resource type strings: the first element
 * is the resource type name of the first-order resource of this planner,
 * the second is the second-order, and so on and so forth. len is the
 * size of this array, and this must not exceed the resource dimension
 * set for this planner.
 *
 *     - ctx: the opaque planner context returned from planner_new
 *     - rts: an array of resource type strings
 *     - len: the length of rts
 */
int planner_set_resrc_types (planner_t *ctx, const char **rts, size_t len);

/* Return the name of the resource type corresponding to the i_th order
 * resource.
 *
 *     - ctx: the opaque planner context returned from planner_new
 *     - i: order index of the target resource
 */
const char *planner_resrc_index2type (planner_t *ctx, int i);

/* Return the index of the resource type name, t
 *
 *     - ctx: the opaque planner context returned from planner_new
 *     - t: the name string of the resource type
 */
int planner_resrc_type2index (planner_t *ctx, const char *t);

/* Find the earliest point in time when the request can be reserved
 * and return that time. Note that this only returns a point at which
 * resource state changes. In other words, if the number of available
 * resources change at t1 and t2, the possible returns are only t1 and
 * t2, not t1+1 or t1+2 even if the latter points also satisfy the
 * request. Return -1 on error and set errno.
 *
 *     - ctx: the opaque planner context returned from planner_new
 *     - req: request specifying the resource amounts and duration
 *            duration must be greater than or equal to 1 (time units)
 */
int64_t planner_avail_time_first (planner_t *ctx, req_t *req);

/* Find the next earliest point in time for the same request queried
 * before through either planner_avail_time_first or planner_avail_time_next
 * and and return that time. Note that this only returns a point at which
 * resource state changes. In other words, if the number of available
 * resources change at t1 and t2, the possible returns are only t1 and
 * t2, not t1+1 or t1+2 even if the latter points also satisfy the
 * request. Return -1 on error and set errno.
 *
 *     - ctx: the opaque planner context returned from planner_new
 */
int64_t planner_avail_time_next (planner_t *ctx);

/* Return 0 if the given request consisting of numbers of resources and
 * duration can be satisfied at starttime. Unlike planner_avail_time*
 * functions, this works with an arbirary time within the valid
 * planner span. Return -1 if the request cannot be satisfied or an error
 * is encountered in which case errno is set.
 *
 *     - ctx: the opaque planner context returned from planner_new
 *     - starttime: start time at which the resource request must
 *            be available
 *     - req: request specifying the resource amounts and duration. duration
 *            must be greater than or equal to 1 (time unit)
 */
int planner_avail_resources_at (planner_t *ctx, int64_t starttime, req_t *req);

/* Allocate and return an object of reservation_t (opaque) type, being built
 * of the passed-in plan. The object must be freed using
 * planner_reservation_destroy when it is not needed.
 *
 *     - ctx: the opaque planner context returned from planner_new
 *     - plan: describe the resource and duration requests. The start
 *           time of this request should have been previously determined
 *           to be satisfiable by the planner_avail_time_* functions above.
 *           Duration request in the plan must be greater than or equal
 *           to 2 (time units) as a reservation is represented as two
 *           unique time points.
 */
reservation_t *planner_reservation_new (planner_t *ctx, plan_t *plan);

/* Add a new reservation to the planner and update the planner's
 * resource/schduled-point state. It resets the planner's iterator
 * so that planner_avail_time_next will be made to return the
 * earliest schedulable point.
 *
 * Return -1 on error and set errno. User should check and print
 * errno if -1. Otherwise return 0.
 *
 * EINVAL: invalid argument
 * EKEYREJECTED: can't update planner's internal data structures
 * ERANGE: resource state became out of range e.g., reserving more than
 *     what is available: rsv wasn't created with available time returnedi
 *     and thus validated using a planner_avail famility function)?
 *
 *     - ctx: the opaque planner context returned from planner_new
 *     - rsv: new reservation.
 *     - validate: if 1 is passed, extra check is performed if rsv is
 *            a valid reservation.
 */
int planner_add_reservation (planner_t *ctx, reservation_t *rsv, int validate);

/* Remove the existing reservation from the planner and update its
 * state. It resets the planner's iterator such that planner_avail_time_next
 * will be made to return the earliest schedulable point.
 *
 * Return -1 on error and set errno; otherwise return 0.
 *
 * EINVAL: invalid argument
 * EKEYREJECTED: can't update one of planner's internal data structures
 * ERANGE: resource state became invalid. e.g., reserving more than
 *     what is available: rsv wasn't created with available time returnedi
 *     and thus validated using a planner_avail famility function)?
 *
 *     - ctx: the opaque planner context returned from planner_new
 *     - rsv: an existing reservation
 */
int planner_rem_reservation (planner_t *ctx, reservation_t *rsv);

/* Destroy the reservation object. If rsv has not been removed (using
 * planner_rem_reservation), this call first removes the rsv before
 * deallocating its memory.
 *
 *     - ctx: the opaque planner context returned from planner_new
 *     - rsv_p: a pointer to the reservation object returned
 *           from planner_reservation_new
 */
void planner_reservation_destroy (planner_t *ctx, reservation_t **rsv_p);

/* Return the reservation with the earliest start time. One should
 * use this function to get the first reservation from which to iterate
 * through subsequent reservations. This scheme allows you to
 * iterate through the reservations sorted in starting time order.
 *
 *     - ctx: the opaque planner context returned from planner_new
 */
reservation_t *planner_reservation_first (planner_t *ctx);

/* Return the next reservation planned in the planner. Please see the
 * comments above for planner_reservation_first. planner_reservation_next
 * returns the reservation that appears right after rsv in start-time
 * sorted order.
 *
 *     - ctx: the opaque planner context returned from planner_new
 *     - rsv: a reservation object returned previously
 */
reservation_t *planner_reservation_next (planner_t *ctx, reservation_t *rsv);

/* Return the reservation keyed by the id. id is the id field
 * of the plan_t field given to planner_reservation_new.
 * Return NULL when no reservation by id exists.
 */
reservation_t *planner_reservation_by_id (planner_t *ctx, int64_t id);
reservation_t *planner_reservation_by_id_str (planner_t *ctx, const char *str);

/* Return 0 if rsv has been added to the planner; otherwise -1
 */
int planner_reservation_added (planner_t *ctx, reservation_t *rsv);

/* Return a string containing the information on a reservation. The
 * returned string must be deallocated by the caller using free.
 *
 *     - ctx: the opaque planner context returned from planner_new
 *     - rsv: a reservation object
 */
char *planner_reservation_to_string (planner_t *ctx, reservation_t *rsv);

/* Getters for reservation_t:
 */
int64_t planner_reservation_starttime (planner_t *ctx, reservation_t *rsv);
int64_t planner_reservation_endtime (planner_t *ctx, reservation_t *rsv);
const uint64_t *planner_reservation_reserved (planner_t *ctx,
                    reservation_t *rsv, size_t *len);

/* Print the planner information in the files that can be visualized using gnuplot
 *
 *     - ctx: the opaque planner context returned from planner_new
 *     - base_fname: base filename (<base_fname>.csv and <base_fname>.gp)
 *           to render: % gnuplot <base_fname>.gp > planner_out.png
 *     - d: which resource dimension to print
 */
int planner_print_gnuplot (planner_t *ctx, const char *base_fname, size_t d);

#endif /* PLANNER_H */

/*
 * vi: ts=4 sw=4 expandtab
 */
