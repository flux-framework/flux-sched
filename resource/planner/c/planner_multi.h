/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef PLANNER_MULTI_H
#define PLANNER_MULTI_H

#include "planner.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct planner_multi_t planner_multi_t;

/*! Construct a planner_multi_t context that creates and manages len number of
 *  planner_t objects. Individual planner_t context can be accessed via
 *  planner_multi_at (i). Index i corresponds to the resource type of
 *  i^th element of resource_types array.
 *
 *  \param base_time    earliest schedulable point expressed in integer time
 *                      (i.e., the base time of the planner to be constructed).
 *  \param duration     time span of this planner_multi (i.e., all planned spans
 *                      must end before base_time + duration).
 *  \param resource_totals
 *                      64-bit unsigned integer array of size len where each
 *                      element contains the total count of available resources
 *                      of a single resource type.
 *  \param resource_types
 *                      string array of size len where each element contains
 *                      the resource type corresponding to each corresponding
 *                      element in the resource_totals array.
 *  \param len          length of the resource_totals and resource_types arrays.
 *
 *  \return             a new planner_multi context; NULL on error with errno
 *                      set as follows:
 *                          EINVAL: invalid argument.
 *                          ERANGE: resource_totals contains an out-of-range
 *                                  value.
 */
planner_multi_t *planner_multi_new (int64_t base_time,
                                    uint64_t duration,
                                    const uint64_t *resource_totals,
                                    const char **resource_types,
                                    size_t len);

/*! Initialize empty planner_multi.
 *
 *  \return             new planner_multi context; NULL on an error with errno set
 *                      as follows:
 *                      pointer to a planner_multi_t object on success; -1 on
 *                      an error with errno set.
 */
planner_multi_t *planner_multi_empty ();

/*! Copy a planner_multi_t.
 *
 *  \param mp           planner to copy.
 *
 *  \return             a new planner_multi context copied from mp; NULL on error
 *                      with errno set as follows:
 *                          ENOMEM: memory error.
 */
planner_multi_t *planner_multi_copy (planner_multi_t *mp);

/*! Assign a planner_multi_t.
 *
 *  \param lhs          the base planner_multi which will be assigned to rhs.
 *  \param rhs          the base planner_multi which will be copied and returned as
 *                      a new planner_multi context.
 *
 */
void planner_multi_assign (planner_multi_t *lhs, planner_multi_t *rhs);

/*! Getters:
 *  \return             -1 or NULL on an error with errno set as follows:
 *                         EINVAL: invalid argument.
 */
int64_t planner_multi_base_time (planner_multi_t *ctx);
int64_t planner_multi_duration (planner_multi_t *ctx);
size_t planner_multi_resources_len (planner_multi_t *ctx);
const char *planner_multi_resource_type_at (planner_multi_t *ctx, unsigned int i);
const uint64_t *planner_multi_resource_totals (planner_multi_t *ctx);
int64_t planner_multi_resource_total_at (planner_multi_t *ctx, unsigned int i);
int64_t planner_multi_resource_total_by_type (planner_multi_t *ctx, const char *resource_type);

/*! Reset the planner_multi_t context with a new time bound.
 *  Destroy all existing planned spans in its managed planner_t objects.
 *
 *  \param ctx          an opaque planner_multi_t context returned from
 *                      planner_multi_new.
 *  \param base_time    the earliest schedulable point expressed in integer time
 *                      (i.e., the base time of the planner to be constructed).
 *  \param duration     the time span of this planner (i.e., all planned spans
 *                      must end before base_time + duration).
 *  \return             0 on success; -1 on error with errno set as follows:
 *                          EINVAL: invalid argument.
 */
int planner_multi_reset (planner_multi_t *ctx, int64_t base_time, uint64_t duration);

/*! Destroy the planner_multi.
 *
 *  \param ctx_p        a pointer to a planner_multi_t context pointer returned
 *                      from planner_new
 *
 */
void planner_multi_destroy (planner_multi_t **ctx_p);

/*! Return the i^th planner object managed by the planner_multi_t ctx.
 *  Index i corresponds to the resource type of i^th element of
 *  resource_types array passed in via planner_multi_new ().
 *
 *  \param ctx          an opaque planner_multi_t context returned from
 *                      planner_multi_new.
 *  \param i            planner array index
 *  \return             a planner_t context pointer on success; NULL on error
 *                      with errno set as follows:
 *                          EINVAL: invalid argument.
 */
planner_t *planner_multi_planner_at (planner_multi_t *ctx, unsigned int i);

/*! Find and return the earliest point in integer time when the request can be
 *  satisfied.
 *
 *  Note on semantics: this function returns a time point where the resource state
 *  changes. If the number of available resources change at time point t1 and
 *  t2 (assuming t2 is greater than t1+2), the possible schedule points that this
 *  function can return is either t1 or t2, but not t1+1 nor t1+2 even if these
 *  points also satisfy the request.
 *
 *  \param ctx          an opaque planner_multi_t context returned from
 *                      planner_multi_new.
 *  \param on_or_after  available on or after the specified time.
 *  \param duration     requested duration; must be greater than or equal to 1.
 *  \param resource_requests
 *                      64-bit unsigned integer array of size len specifying
 *                      the requested resource counts.
 *  \param len          length of resource_counts and resource_types arrays.
 *  \return             the earliest time at which the resource request
 *                      can be satisfied;
 *                      -1 on error with errno set as follows:
 *                          EINVAL: invalid argument.
 *                          ERANGE: resource_counts contain an out-of-range value.
 *                          ENOENT: no scheduleable point
 */
int64_t planner_multi_avail_time_first (planner_multi_t *ctx,
                                        int64_t on_or_after,
                                        uint64_t duration,
                                        const uint64_t *resource_requests,
                                        size_t len);

/*! Find and return the next earliest point in time at which the same request
 *  queried before via either planner_avail_time_first or
 *  planner_multi_avail_time_next can be satisfied.  Same semantics as
 *  planner_multi_avail_time_first.
 *
 *  \param ctx          an opaque planner_multi_t context returned from
 *                      planner_multi_new.
 *  \return             the next earliest time at which the resource request
 *                      can be satisfied;
 *                      -1 on error with errno set as follows:
 *                          EINVAL: invalid argument.
 *                          ERANGE: request out of range
 *                          ENOENT: no scheduleable point
 */
int64_t planner_multi_avail_time_next (planner_multi_t *ctx);

/*! Return how many resources of ith type is available at the given time.
 *
 *  \param ctx          opaque planner context returned from planner_multi_new.
 *  \param at           instant time for which this query is made.
 *  \param i            index of the resource type to queried
 *  \return             available resource count; -1 on an error with errno set
 *                      as follows:
 *                          EINVAL: invalid argument.
 */
int64_t planner_multi_avail_resources_at (planner_multi_t *ctx, int64_t at, unsigned int i);

/*! Return how many resources are available at the given instant time (at).
 *
 *  \param ctx          an opaque planner_multi_t context returned from
 *                      planner_multi_new.
 *  \param at           instant time for which this query is made.
 *  \param resource_counts
 *                      resources array buffer to copy and return available
 *                      counts into.
 *  \param len          length of resources array.
 *  \return             0 on success; -1 on error with errno set as follows:
 *                          EINVAL: invalid argument.
 */
int planner_multi_avail_resources_array_at (planner_multi_t *ctx,
                                            int64_t at,
                                            int64_t *resource_counts,
                                            size_t len);

/*! Test if the given resource request can be satisfied at the start time.
 *  Note on semantics: Unlike planner_multi_avail_time* functions, this function
 *  can be used to test an arbitrary time span.
 *
 *  \param ctx          an opaque planner_multi_t context returned from
 *                      planner_multi_new.
 *  \param at           start time from which the requested resources must
 *                      be available for duration.
 *  \param duration     requested duration; must be greater than or equal to 1.
 *  \param resource_requests
 *                      64-bit unsigned integer array of size len specifying
 *                      the requested resource counts.
 *  \param len          length of resource_counts and resource_types arrays.
 *  \return             0 if the request can be satisfied; -1 if it cannot
 *                      be satisfied or an error encountered (errno as follows):
 *                          EINVAL: invalid argument.
 *                          ERANGE: resource_counts contain an out-of-range value.
 *                          ENOTSUP: internal error encountered.
 */
int planner_multi_avail_during (planner_multi_t *ctx,
                                int64_t at,
                                uint64_t duration,
                                const uint64_t *resource_requests,
                                size_t len);

/*! Return how many resources are available for the duration starting from at.
 *
 *  \param ctx          an opaque planner_multi_t context returned from
 *                      planner_multi_new.
 *  \param at           instant time for which this query is made.
 *  \param duration     requested duration; must be greater than or equal to 1.
 *  \param resource_counts
 *                      resources array buffer to copy and return available counts
 *                      into.
 *  \param len          length of resource_counts and resource_types arrays.
 *  \return             0 on success; -1 on an error with errno set as follows:
 *                          EINVAL: invalid argument.
 */

int planner_multi_avail_resources_array_during (planner_multi_t *ctx,
                                                int64_t at,
                                                uint64_t duration,
                                                int64_t *resource_counts,
                                                size_t len);

/*! Add a new span to the multi-planner and update the planner's resource/time
 *  state. Reset the multi-planner's iterator so that
 *  planner_multi_avail_time_next will be made to return the earliest
 *  schedulable point.
 *
 *  \param ctx          opaque planner_multi_t context returned
 *                      from planner_multi_new.
 *  \param start_time   start time from which the resource request must
 *                      be available for duration.
 *  \param duration     requested duration; must be greater than or equal to 1.
 *  \param resource_requests
 *                      resource counts request.
 *  \param len          length of requests.
 *
 *  \return             span id on success; -1 on error with errno set
 *                      as follows:
 *                          EINVAL: invalid argument.
 *                          EKEYREJECTED: can't update planner's internal data.
 *                          ERANGE: a resource state became out of a valid
 *                                  range, e.g., reserving more than available.
 */
int64_t planner_multi_add_span (planner_multi_t *ctx,
                                int64_t start_time,
                                uint64_t duration,
                                const uint64_t *resource_requests,
                                size_t len);

/*! Remove the existing span from multi-planner and update resource/time state.
 *  Reset the planner's iterator such that planner_avail_time_next will be made
 *  to return the earliest schedulable point.
 *
 *  \param ctx          opaque multi-planner context returned
 *                      from planner_multi_new.
 *  \param span_id      span_id returned from planner_multi_add_span.
 *  \return             0 on success; -1 on error with errno set as follows:
 *                          EINVAL: invalid argument.
 *                          EKEYREJECTED: span could not be removed from
 *                                        the planner's internal data structures.
 *                          ERANGE: a resource state became out of a valid range.
 */
int planner_multi_rem_span (planner_multi_t *ctx, int64_t span_id);

/*! Reduce the existing span's resources from the planner.
 *  This function will be called for a partial release/cancel.
 *  If the number of resources to be removed is equal to those
 *  allocated to the span, completely remove the span.
 *
 *  \param ctx          opaque multi-planner context returned
 *                      from planner_multi_new.
 *  \param span_id      span_id returned from planner_add_span.
 *  \param reduced_totals
 *                      64-bit unsigned integer array of size len where each
 *                      element contains the total count of available resources
 *                      of a single resource type.
 *  \param resource_types
 *                      string array of size len where each element contains
 *                      the resource type corresponding to each corresponding
 *                      element in the resource_totals array.
 *  \param len          length of the resource_totals and resource_types arrays.
 *  \param removed      bool indicating if the entire span was removed.
 *  \return             0 on success; -1 on error with errno set as follows:
 *                          EINVAL: invalid argument.
 *                          EKEYREJECTED: span could not be removed from
 *                                        the planner's internal data structures.
 *                          ERANGE: a resource state became out of a valid range.
 */
int planner_multi_reduce_span (planner_multi_t *ctx,
                               int64_t span_id,
                               const uint64_t *reduced_totals,
                               const char **resource_types,
                               size_t len,
                               bool &removed);

/*! Get the number of used resources of resource index i corresponding
 *  to the provided span id.
 *
 *  \param ctx          opaque multi-planner context returned
 *                      from planner_multi_new.
 *  \param span_id      span_id returned from planner_add_span.
 *  \param i            index of the resource type to queried
 */
int64_t planner_multi_span_planned_at (planner_multi_t *ctx, int64_t span_id, unsigned int i);

//! Span iterators -- there is no specific iteration order
//  return -1 when you no longer can iterate: EINVAL when ctx is NULL.
//  ENOENT when you reached the end of the spans
int64_t planner_multi_span_first (planner_multi_t *ctx);
int64_t planner_multi_span_next (planner_multi_t *ctx);

size_t planner_multi_span_size (planner_multi_t *ctx);

/*
 *  Returns true if all the member variables and objects are equal.
 *  Used by testsuite.
 */
bool planner_multis_equal (planner_multi_t *lhs, planner_multi_t *rhs);

/*! Update the counts and resource types to support elasticity.
 *
 *  \param ctx          opaque multi-planner context returned
 *                      from planner_multi_new.
 *  \param resource_totals
 *                      64-bit unsigned integer array of size len where each
 *                      element contains the total count of available resources
 *                      of a single resource type.
 *  \param resource_types
 *                      string array of size len where each element contains
 *                      the resource type corresponding to each corresponding
 *                      element in the resource_totals array.
 *  \param len          length of resource_counts and resource_types arrays.
 *  \return             0 on success; -1 on an error with errno set as follows:
 *                          EINVAL: invalid argument.
 */
int planner_multi_update (planner_multi_t *ctx,
                          const uint64_t *resource_totals,
                          const char **resource_types,
                          size_t len);

#ifdef __cplusplus
}
#endif

#endif /* PLANNER_MULTI_H */

/*
 * vi: ts=4 sw=4 expandtab
 */
