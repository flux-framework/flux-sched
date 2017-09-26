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

#ifndef PLANNER_H
#define PLANNER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLANNER_NUM_TYPES 5

typedef struct planner planner_t;

/*! Construct a planner.
 *
 *  \param base_time    earliest schedulable point expressed in integer time
 *                      (i.e., the base time of the planner to be constructed).
 *  \param duration     time span of this planner (i.e., all planned spans
 *                      must end before base_time + duration).
 *  \param resource_totals
 *                      64-bit unsigned integer array of size len where each
 *                      element contains the total count of available resources
 *                      of a single resource type. Up to PLANNER_NUM_TYPES types
 *                      are supported.
 *  \param resource_types
 *                      string array of size len where each element contains
 *                      the resource type corresponding to each corresponding
 *                      element in the resource_totals array.
 *  \param len          length of the resource_totals and resource_types arrays;
 *                      must not exceed PLANNER_NUM_TYPES.
 *  \return             new planner context; NULL on an error with errno set
 *                      as follows:
 *                      0 on success; -1 on an error with errno set:
 *                          EINVAL: invalid argument.
 *                          ERANGE: resource_totals contains an out-of-range value.
 */
planner_t *planner_new (int64_t base_time, uint64_t duration,
                        const uint64_t *resource_totals,
                        const char **resource_types, size_t len);

/*! Reset the planner with a new time bound. Destroy all existing planned spans.
 *
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
size_t planner_resources_len (planner_t *ctx);
int64_t planner_resource_total_at (planner_t *ctx, unsigned int i);
int64_t planner_resource_total_by_type (planner_t *ctx,
                                        const char *resource_type);
const char **planner_resource_types (planner_t *ctx);
int planner_resource_index_of_type (planner_t *ctx,
                                    const char *resource_type);
const char *planner_resource_type_at (planner_t *ctx, unsigned int i);

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
 *  \param resource_counts
 *                      64-bit unsigned integer array of size len specifying
 *                      the requested resource counts.
 *  \param len          length of resource_counts and resource_types arrays;
 *                      must not exceed PLANNER_NUM_TYPES.
 *  \return             earliest time at which the request can be satisfied;
 *                      -1 on an error with errno set as follows:
 *                          EINVAL: invalid argument.
 *                          ERANGE: resource_counts contain an out-of-range value.
 */
int64_t planner_avail_time_first (planner_t *ctx, int64_t on_or_after,
                                  uint64_t duration,
                                  const uint64_t *resource_counts, size_t len);

/*! Find and return the next earliest point in time at which the same request
 *  queried before via either planner_avail_time_first or
 *  planner_avail_time_next can be satisfied.  Same semantics as
 *  planner_avail_time_first.
 *
 *  \param ctx          opaque planner context returned from planner_new.
 *  \return             earliest time at which the request can be satisfied;
 *                      -1 on an error with errno set as follows:
 *                          EINVAL: invalid argument.
 */
int64_t planner_avail_time_next (planner_t *ctx);

/*! Test if the given request can be satisfied at start_time.
 *  Note on semantics: Unlike planner_avail_time* functions, this function
 *  can be used to test an arbitrary time span.
 *
 *  \param ctx          opaque planner context returned from planner_new.
 *  \param at           start time from which the requested resources must
 *                      be available for duration.
 *  \param duration     requested duration; must be greater than or equal to 1.
 *  \param resource_counts
 *                      64-bit unsigned integer array of size len specifying
 *                      the requested resource counts.
 *  \param len          length of resource_counts and resource_types arrays.
 *                      must not exceed PLANNER_NUM_TYPES.
 *  \return             earliest time at which the request can be satisfied;
 *                      -1 on an error with errno set as follows:
 *                          EINVAL: invalid argument.
 *                          ERANGE: resource_counts contain an out-of-range value.
 *                          ENOTSUP: internal error encountered.
 */
int planner_avail_during (planner_t *ctx, int64_t at, uint64_t duration,
                          const uint64_t *resource_counts, size_t len);

/*! Return how many ith resources are available for the duration starting from at.
 *
 *  \param ctx          opaque planner context returned from planner_new.
 *  \param at           instant time for which this query is made.
 *  \param duration     requested duration; must be greater than or equal to 1.
 *  \param i            index of the resource type to queried; must be less than
 *                      PLANNER_NUM_TYPES.
 *  \return             available resource count; -1 on an error with errno set
 *                      as follows:
 *                          EINVAL: invalid argument.
 */
int64_t planner_avail_resources_during (planner_t *ctx, int64_t at,
                                        uint64_t duration, unsigned int i);

/*! Return how many resources of resource_type are available for the duration
 *  starting from at.
 *
 *
 *  \param ctx          opaque planner context returned from planner_new.
 *  \param at           instant time for which this query is made.
 *  \param duration     requested duration; must be greater than or equal to 1.
 *  \param resource_type
 *                      the resource type to queried.
 *  \return             available resource count; -1 on an error with errno set
 *                      as follows:
 *                          EINVAL: invalid argument.
 */
int64_t planner_avail_resources_during_by_type (planner_t *ctx, int64_t at,
                                                uint64_t duration,
                                                const char *resource_type);

/*! Return how many resources are available for the duration starting from at.
 *
 *
 *  \param ctx          opaque planner context returned from planner_new.
 *  \param at           instant time for which this query is made.
 *  \param duration     requested duration; must be greater than or equal to 1.
 *  \param resources    resources array buffer to copy and return available counts
 *                      into.
 *  \return             available resource count; -1 on an error with errno set
 *                      as follows:
 *                          EINVAL: invalid argument.
 */
int planner_avail_resources_array_during (planner_t *ctx, int64_t at,
                                          uint64_t duration, int64_t *resources,
                                          size_t len);

/*! Return how many resources of ith type is available at the given time.
 *
 *  \param ctx          opaque planner context returned from planner_new.
 *  \param at           instant time for which this query is made.
 *  \param i            index of the resource type to queried; must be less than
 *                      PLANNER_NUM_TYPES.
 *  \return             available resource count; -1 on an error with errno set
 *                      as follows:
 *                          EINVAL: invalid argument.
 */
int64_t planner_avail_resources_at (planner_t *ctx, int64_t at, unsigned int i);


/*! Return how many resources of resource_type are available at the given instant
 *  time.
 *
 *  \param ctx          opaque planner context returned from planner_new.
 *  \param at           instant time for which this query is made.
 *  \param resource_type
 *                      the resource type to be queried.
 *  \return             available resource count; -1 on an error with errno set
 *                      as follows:
 *                          EINVAL: invalid argument.
 */
int64_t planner_avail_resources_at_by_type (planner_t *ctx, int64_t at,
                                            const char *resource_type);


/*! Return how many resources are available at the given instant time.
 *
 *  \param ctx          opaque planner context returned from planner_new.
 *  \param at           instant time for which this query is made.
 *  \param resources    resources array buffer to copy and return available
 *                      counts into.
 *  \param len          length of resources array.
 *  \return             0 on success; -1 on an error with errno set as follows:
 *                          EINVAL: invalid argument.
 */
int planner_avail_resources_array_at (planner_t *ctx, int64_t at,
                                      int64_t *resources, size_t len);


/*! Add a new span to the planner and update the planner's resource/time state.
 *  Reset the planner's iterator so that planner_avail_time_next will be made
 *  to return the earliest schedulable point.
 *
 *  \param ctx          opaque planner context returned from planner_new.
 *  \param start_time   start time from which the resource request must
 *                      be available for duration.
 *  \param duration     requested duration; must be greater than or equal to 1.
 *  \param resource_counts
 *                      64-bit unsigned integer array of size len specifying
 *                      the requested resource counts.
 *  \param len          length of resource_counts and resource_types array;
 *                      must not exceed PLANNER_NUM_TYPES.
 *
 *  \return             span id on success; -1 on an error with errno set as follows:
 *                          EINVAL: invalid argument.
 *                          EKEYREJECTED: can't update planner's internal data.
 *                          ERANGE: a resource state became out of a valid range,
 *                                  e.g., reserving more than available.
 */
int64_t planner_add_span (planner_t *ctx, int64_t start_time, uint64_t duration,
                          const uint64_t *resource_counts, size_t len);

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

//! Span iterators -- there is no specific iteration order
int64_t planner_span_first (planner_t *ctx);
int64_t planner_span_next (planner_t *ctx);
size_t planner_span_size (planner_t *ctx);

//! Return 0 if the span has been inserted and active in the planner
bool planner_is_active_span (planner_t *ctx, int64_t span_id);

//! Getters for span. Return -1 on an error.
int64_t planner_span_start_time (planner_t *ctx, int64_t span_id);
int64_t planner_span_duration (planner_t *ctx, int64_t span_id);
int64_t planner_span_id (planner_t *ctx, int64_t span_id);
int64_t planner_span_resource_count_at (planner_t *ctx, int64_t span_id,
                                        unsigned int at);
int64_t planner_span_resource_count_by_type (planner_t *ctx, int64_t span_id,
                                             const char *resource_type);

#ifdef __cplusplus
}
#endif

#endif /* PLANNER_H */

/*
 * vi: ts=4 sw=4 expandtab
 */
