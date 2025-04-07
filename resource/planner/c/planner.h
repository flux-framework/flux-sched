/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef PLANNER_H
#define PLANNER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct planner_t planner_t;

/*! Construct a planner.
 *
 *  \param base_time    earliest schedulable point expressed in integer time
 *                      (i.e., the base time of the planner to be constructed).
 *  \param duration     time span of this planner (i.e., all planned spans
 *                      must end before base_time + duration).
 *  \param resource_total
 *                      the total count of available resources.
 *  \param resource_type
 *                      the resource type string
 *  \return             new planner context; NULL on an error with errno set
 *                      as follows:
 *                      pointer to a planner_t object on success; -1 on
 *                      an error with errno set:
 *                          EINVAL: invalid argument.
 *                          ERANGE: resource_total is an out-of-range value.
 */
planner_t *planner_new (int64_t base_time,
                        uint64_t duration,
                        uint64_t resource_total,
                        const char *resource_type);

/*! Initialize empty planner.
 *
 *  \return             new planner context; NULL on an error with errno set
 *                      as follows:
 *                      pointer to a planner_t object on success; -1 on
 *                      an error with errno set.
 */
planner_t *planner_new_empty ();

/*! Copy a planner.
 *
 *  \param p            the base planner which will be copied and returned as
 *                      a new planner context.
 *  \return             new planner context; NULL on an error with errno set
 *                      as follows:
 *                      pointer to a planner_t object on success; -1 on
 *                      an error with errno set.
 */
planner_t *planner_copy (planner_t *p);

/*! Assign a planner.
 *
 *  \param lhs          the base planner which will be assigned to rhs.
 *  \param rhs          the base planner which will be copied and returned as
 *                      a new planner context.
 *
 */
void planner_assign (planner_t *lhs, planner_t *rhs);

/*! Reset the planner with a new time bound. Destroy all existing planned spans.
 *
 *  \param ctx          opaque planner context returned from planner_new.
 *  \param base_time    earliest schedulable point expressed in integer time
 *                      (i.e., the base time of the planner to be constructed).
 *  \param duration     time span of this planner (i.e., all planned spans
 *                      must end before base_time + duration).
 *  \return             0 on success; -1 on an error with errno set as follows:
 *                          EINVAL: invalid argument.
 */
int planner_reset (planner_t *ctx, int64_t base_time, uint64_t duration);

/*! Destroy the planner.
 *
 *  \param ctx_p        pointer to a planner context pointer returned.
 *                      from planner_new
 *
 */
void planner_destroy (planner_t **ctx_p);

/*! Getters:
 *  \return             -1 or NULL on an error with errno set as follows:
 *                         EINVAL: invalid argument.
 */
int64_t planner_base_time (planner_t *ctx);
int64_t planner_duration (planner_t *ctx);
int64_t planner_resource_total (planner_t *ctx);
const char *planner_resource_type (planner_t *ctx);

/*! Find and return the earliest point in integer time when the request can be
 *  satisfied.
 *
 *  Note on semantics: this function returns a time point where the resource state
 *  changes. If the number of available resources change at time point t1 and
 *  t2 (assuming t2 is greater than t1+2), the possible schedule points that this
 *  function can return is either t1 or t2, but not t1+1 nor t1+2 even if these
 *  points also satisfy the request.
 *
 *  \param ctx          opaque planner context returned from planner_new.
 *  \param on_or_after  available on or after the specified time.
 *  \param duration     requested duration; must be greater than or equal to 1.
 *  \param request      resource count request.
 *  \return             earliest time at which the request can be satisfied;
 *                      -1 on an error with errno set as follows:
 *                          EINVAL: invalid argument.
 *                          ERANGE: request is an out-of-range value.
 *                          ENOENT: no scheduleable point
 */
int64_t planner_avail_time_first (planner_t *ctx,
                                  int64_t on_or_after,
                                  uint64_t duration,
                                  uint64_t request);

/*! Find and return the next earliest point in time at which the same request
 *  queried before via either planner_avail_time_first or
 *  planner_avail_time_next can be satisfied.  Same semantics as
 *  planner_avail_time_first.
 *
 *  \param ctx          opaque planner context returned from planner_new.
 *  \return             earliest time at which the request can be satisfied;
 *                      -1 on error with errno set as follows:
 *                          EINVAL: invalid argument.
 *                          ERANGE: request is out of range
 *                          ENOENT: no scheduleable point
 */
int64_t planner_avail_time_next (planner_t *ctx);

/*! Test if the given request can be satisfied at the start time.
 *  Note on semantics: Unlike planner_avail_time* functions, this function
 *  can be used to test an arbitrary time span.
 *
 *  \param ctx          opaque planner context returned from planner_new.
 *  \param at           start time from which the requested resources must
 *                      be available for duration.
 *  \param duration     requested duration; must be greater than or equal to 1.
 *  \param request      resource count request.
 *  \return             0 if the request can be satisfied; -1 if it cannot
 *                      be satisfied or an error encountered (errno as follows):
 *                          EINVAL: invalid argument.
 *                          ERANGE: request is an out-of-range value.
 *                          ENOTSUP: internal error encountered.
 */
int planner_avail_during (planner_t *ctx, int64_t at, uint64_t duration, uint64_t request);

/*! Return how resources are available for the duration starting from at.
 *
 *  \param ctx          opaque planner context returned from planner_new.
 *  \param at           instant time for which this query is made.
 *  \param duration     requested duration; must be greater than or equal to 1.
 *  \return             available resource count; -1 on an error with errno set
 *                      as follows:
 *                          EINVAL: invalid argument.
 */
int64_t planner_avail_resources_during (planner_t *ctx, int64_t at, uint64_t duration);

/*! Return how many resources are available at the given time.
 *
 *  \param ctx          opaque planner context returned from planner_new.
 *  \param at           instant time for which this query is made.
 *  \return             available resource count; -1 on an error with errno set
 *                      as follows:
 *                          EINVAL: invalid argument.
 */
int64_t planner_avail_resources_at (planner_t *ctx, int64_t at);

/*! Add a new span to the planner and update the planner's resource/time state.
 *  Reset the planner's iterator so that planner_avail_time_next will be made
 *  to return the earliest schedulable point.
 *
 *  \param ctx          opaque planner context returned from planner_new.
 *  \param start_time   start time from which the resource request must
 *                      be available for duration.
 *  \param duration     requested duration; must be greater than or equal to 1.
 *  \param request      resource count request.
 *
 *  \return             span id on success; -1 on an error with errno set as follows:
 *                          EINVAL: invalid argument.
 *                          EKEYREJECTED: can't update planner's internal data.
 *                          ERANGE: a resource state became out of a valid range,
 *                                  e.g., reserving more than available.
 */
int64_t planner_add_span (planner_t *ctx, int64_t start_time, uint64_t duration, uint64_t request);

/*! Remove the existing span from the planner and update its resource/time state.
 *  Reset the planner's iterator such that planner_avail_time_next will be made
 *  to return the earliest schedulable point.
 *
 *  \param ctx          opaque planner context returned from planner_new.
 *  \param span_id      span_id returned from planner_add_span.
 *  \return             0 on success; -1 on an error with errno set as follows:
 *                          EINVAL: invalid argument.
 *                          EKEYREJECTED: span could not be removed from
 *                                        the planner's internal data structures.
 *                          ERANGE: a resource state became out of a valid range.
 */
int planner_rem_span (planner_t *ctx, int64_t span_id);

/*! Reduce the existing span's resources from the planner.
 *  This function will be called for a partial release/cancel.
 *  If the number of resources to be removed is equal to those
 *  allocated to the span, completely remove the span.
 *
 *  \param ctx          opaque planner context returned from planner_new.
 *  \param span_id      span_id returned from planner_add_span.
 *  \param to_remove    number of resources to free from the span
 *  \param removed      bool indicating if the entire span was removed.
 *  \return             0 on success; -1 on an error with errno set as follows:
 *                          EINVAL: invalid argument.
 *                          EKEYREJECTED: span could not be removed from
 *                                        the planner's internal data structures.
 *                          ERANGE: a resource state became out of a valid range.
 */
int planner_reduce_span (planner_t *ctx, int64_t span_id, int64_t to_remove, bool &removed);

//! Span iterators -- there is no specific iteration order
int64_t planner_span_first (planner_t *ctx);
int64_t planner_span_next (planner_t *ctx);
size_t planner_span_size (planner_t *ctx);

//! Return 0 if the span has been inserted and active in the planner
bool planner_is_active_span (planner_t *ctx, int64_t span_id);

//! Getters for span. Return -1 on an error.
int64_t planner_span_start_time (planner_t *ctx, int64_t span_id);
int64_t planner_span_duration (planner_t *ctx, int64_t span_id);
int64_t planner_span_resource_count (planner_t *ctx, int64_t span_id);

/*
 *  Returns true if all the member variables and objects are equal.
 *  Used by testsuite.
 */
bool planners_equal (planner_t *lhs, planner_t *rhs);

/*! Update the resource count to support elasticity.
 *
 *  \param ctx          opaque planner context returned from planner_new.
 *  \param resource_total
 *                      64-bit unsigned integer of
 *                      the total count of available resources
 *                      of the resource type.
 *  \return             0 on success; -1 on an error with errno set as follows:
 *                          EINVAL: invalid argument.
 */
int planner_update_total (planner_t *ctx, uint64_t resource_total);

#ifdef __cplusplus
}
#endif

#endif /* PLANNER_H */

/*
 * vi: ts=4 sw=4 expandtab
 */
