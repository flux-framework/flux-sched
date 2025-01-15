/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef REAPI_HPP
#define REAPI_HPP

#include <flux/core/job.h>
extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>
#include <flux/idset.h>
#include <jansson.h>
}

#include <cstdint>
#include <string>
#include <optional>
#include "resource/schema/resource_data.hpp"

namespace Flux {
namespace resource_model {

/*! Queue adapter base API class: define a set of methods a queue
 *  policy class (a subclass of this API class) must implement
 *  to be able to work with reapi_t under asynchronous execution.
 */
class queue_adapter_base_t {
   public:
    /*! When a match succeeds, this method is called back
     *  by reapi_t with the matched resource information.
     *  The implementor (e.g., queue policy class) of this method
     *  is expected to dequeue the job from its pending queue
     *  and proceed to the next state transition.
     *
     *  \param jobid     Job ID of the uint64_t type.
     *  \param status    String indicating if the match type is
     *                   allocation or reservation.
     *  \param R         Resource set: i.e., either allocated or reserved.
     *  \param at        If allocated, 0 is returned; if reserved,
     *                   actual time at which the job is reserved.
     *  \param ov        Match performance overhead in terms
     *                   of the elapse time to complete the match operation.
     *  \return          0 on success; -1 on error.
     */
    virtual int handle_match_success (flux_jobid_t jobid,
                                      const char *status,
                                      const char *R,
                                      int64_t at,
                                      double ov) = 0;

    /*! When a match failed (e.g., unsatisfiable jobspec, resource
     *  unavailable, no more jobspec to process), this method is
     *  called back by reapi_t with errcode returned from the resource
     *  match service.
     *  The implementor of this method is expected to dequeue the
     *  the job from the pending job queue, if appropriate, and proceed
     *  to the next pending job or return 0 if the scheduling loop
     *  must be terminated per its queuing policy (e.g., FCFS).
     *
     *  \param           errno returned from the resource match service.
     *                       EBUSY: resource unavailable
     *                       ENODEV: unsatisfiable jobspec
     *                       ENODATA: no more jobspec to process
     *                       Others: one that can raised from match_multi RPC
     *  \return          0 when the loop must terminate; -1 on error.
     */
    virtual int handle_match_failure (flux_jobid_t jobid, int errcode) = 0;

    /*! Return true if the scheduling loop is active under asynchronous
     *  execution; otherwise false.
     */
    virtual bool is_sched_loop_active () = 0;

    /*! Set the state of the scheduling loop.
     *  \param active    true when the scheduling loop becomes
     *                   active; false when becomes inactive.
     *  \return          0 on success; otherwise -1 an error with errno set
     *                   (Note: when the scheduling loop becomes inactive,
     *                    internal queueing can occur and an error can arise):
     *                       - ENOENT (job is not found from some queue)
     *                       - EEXIST (enqueue fails due to an existent entry)
     */
    virtual int set_sched_loop_active (bool active) = 0;
};

/*! High-level resource API base class. Derived classes must implement
 *  the methods.
 */
class reapi_t {
   public:
    /*! Match a jobspec to the "best" resources and either allocate
     *  orelse reserve them. The best resources are determined by
     *  the selected match policy.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module, it is expected to be a pointer
     *                   to a flux_t object.
     *  \param match_op  match_op_t: set to specify the specific match option
     *                   from 1 of 4 choices:
     *                   MATCH_ALLOCATE: try to allocate now and fail if resources
     *                   aren't available.
     *                   MATCH_ALLOCATE_ORELSE_RESERVE : Try to allocate and reserve
     *                   if resources aren't available now.
     *                   MATCH_SATISFIABILITY: Do a satisfiablity check and do not
     *                   allocate.
     *                   MATCH_ALLOCATE_W_SATISFIABILITY: try to allocate and run
     *                   satisfiability check if resources are not available.
     *  \param jobspec   jobspec string.
     *  \param jobid     jobid of the uint64_t type.
     *  \param reserved  Boolean into which to return true if this job has been
     *                   reserved instead of allocated.
     *  \param R         String into which to return the resource set either
     *                   allocated or reserved.
     *  \param at        If allocated, 0 is returned; if reserved, actual time
     *                   at which the job is reserved.
     *  \param ov        Double into which to return performance overhead
     *                   in terms of elapse time needed to complete
     *                   the match operation.
     *  \return          0 on success; -1 on error.
     */
    static int match_allocate (void *h,
                               bool orelse_reserve,
                               const std::string &jobspec,
                               const uint64_t jobid,
                               bool &reserved,
                               std::string &R,
                               int64_t &at,
                               double &ov)
    {
        return -1;
    }

    /*! Multi-Match jobspecs to the "best" resources and either allocate
     *  orelse reserve them. The best resources are determined by
     *  the selected match policy.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module, it is expected to be a pointer
     *                   to a flux_t object.
     *  \param match_op  match_op_t: set to specify the specific match option
     *                   from 1 of 4 choices:
     *                   MATCH_ALLOCATE: try to allocate now and fail if resources
     *                   aren't available.
     *                   MATCH_ALLOCATE_ORELSE_RESERVE : Try to allocate and reserve
     *                   if resources aren't available now.
     *                   MATCH_SATISFIABILITY: Do a satisfiablity check and do not
     *                   allocate.
     *                   MATCH_ALLOCATE_W_SATISFIABILITY: try to allocate and run
     *  \param jobs      JSON array of jobspecs.
     *  \param adapter   queue_adapter_base_t object that provides
     *                   a set of callback methods to be called each time
     *                   the result of a match is returned from the
     *                   resource match service.
     *  \return          0 on success; -1 on error.
     */
    static int match_allocate_multi (void *h,
                                     bool orelse_reserve,
                                     const char *jobs,
                                     queue_adapter_base_t *adapter);

    /*! Update the resource state with R.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module, it is expected to be a pointer
     *                   to a flux_t object.
     *  \param jobid     jobid of the uint64_t type.
     *  \param R         R String of std::string.
     *  \param at        return the scheduled time.
     *  \param ov        return the performance overhead
     *                   in terms of elapse time needed to complete
     *                   the update operation.
     *  \param R_out     return the updated R string.
     *  \return          0 on success; -1 on error.
     */
    static int update_allocate (void *h,
                                const uint64_t jobid,
                                const std::string &R,
                                int64_t &at,
                                double &ov,
                                std::string &R_out)
    {
        return -1;
    }

    /*! Add a subgraph to the resource graph with R_subgraph.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module, it is expected to be a pointer
     *                   to a flux_t object.
     *  \param R_subgraph JGF string of subgraph to attach to existing resources.
     *                   Supports adding a JGF subgraph including the path from the
     *                   cluster root to the subgraph root.
     *  \return          0 on success; -1 on error.
     */
    static int add_subgraph (void *h, const std::string &R_subgraph)
    {
        return -1;
    }

    /*! Remove a subgraph from the resource graph with subgraph_path.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module, it is expected to be a pointer
     *                   to a flux_t object.
     *  \param subgraph_path String representing the path from the cluster root
     *                   to the root of the subgraph to be removed from
     *                   the resource graph
     *  \return          0 on success; -1 on error.
     */
    static int remove_subgraph (void *h, const std::string &subgraph_path)
    {
        return -1;
    }

    /*! Cancel the allocation or reservation corresponding to jobid.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module, it is expected to be a pointer
     *                   to a flux_t object.
     *  \param jobid     jobid of the uint64_t type.
     *  \param noent_ok  don't return an error with nonexistent jobid
     *  \return          0 on success; -1 on error.
     */
    static int cancel (void *h, const uint64_t jobid, bool noent_ok)
    {
        return -1;
    }

    /*! Cancel the allocation or reservation corresponding to jobid.
     *
     *  \param ctx       reapi_module_ctx_t context object
     *  \param jobid     jobid of the uint64_t type.
     *  \param R         R string to remove
     *  \param noent_ok  don't return an error on nonexistent jobid
     *  \param full_removal  bool indicating whether the job is fully canceled
     *  \return          0 on success; -1 on error.
     */
    static int cancel (void *h,
                       const uint64_t jobid,
                       const char *R,
                       bool noent_ok,
                       bool &full_removal)
    {
        return -1;
    }

    /*! Find resources matching the criteria and return them in R format.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module, it is expected to be a pointer
     *                   to a flux_t object.
     *  \param criteria  Search criteria string (e.g., "sched-now=allocated",
     *                   "status=down", etc.)
     *  \param o         JSON object to receive the matching resources in R format
     *                   or nullptr if nothing was matched
     *  \param format    Optional output format string (e.g., "rv1_nosched", "jgf").
     *                   If std::nullopt (default), uses the context's configured
     *                   match_format. If specified, creates a temporary writer
     *                   for that format.
     *  \return          0 on success; -1 on error.
     */
    static int find (void *h,
                     std::string criteria,
                     json_t *&o,
                     std::optional<std::string> format = std::nullopt);

    /*! Get the information on the allocation or reservation corresponding
     *  to jobid.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module, it is expected to be a pointer
     *                   to a flux_t object.
     *  \param jobid     const jobid of the uint64_t type.
     *  \param mode      return string containing the job state.
     *  \param reserved  Boolean into which to return true if this job has been
     *                   reserved instead of allocated.
     *  \param at        If allocated, 0 is returned; if reserved, actual time
     *                   at which the job is reserved.
     *  \param ov        Double into which to return performance overhead
     *                   in terms of elapse time needed to complete
     *                   the match operation.
     *  \return          0 on success; -1 on error.
     */
    static int info (void *h,
                     const uint64_t jobid,
                     std::string &mode,
                     bool &reserved,
                     int64_t &at,
                     double &ov)
    {
        return -1;
    }

    /*! Get the performance information about the resource infrastructure.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module, it is expected to be a pointer
     *                   to a flux_t object.
     *  \param V         Number of resource vertices
     *  \param E         Number of edges
     *  \param J         Number of jobs
     *  \param load      Graph load time
     *  \param min       Min match time
     *  \param max       Max match time
     *  \param avg       Avg match time
     *  \return          0 on success; -1 on error.
     */
    static int stat (void *h,
                     int64_t &V,
                     int64_t &E,
                     int64_t &J,
                     double &load,
                     double &min,
                     double &max,
                     double &avg)
    {
        return -1;
    }

    /*! Set resource status (up or down) for a resource at the specified path.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module, it is expected to be a pointer
     *                   to a flux_t object.
     *  \param resource_path    Path to resource (e.g., "/cluster0/rack0/node3")
     *  \param status    resource_pool_t::status_t::UP or resource_pool_t::status_t::DOWN
     *  \return          0 on success; -1 on error with errno set:
     *                       EINVAL: invalid parameters, resource path not found,
     *                               or unexpected internal error
     */
    static int set_status (void *h,
                           const std::string &resource_path,
                           resource_pool_t::status_t status);

    /*! Get resource status for a resource at the specified path.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module, it is expected to be a pointer
     *                   to a flux_t object.
     *  \param resource_path    Path to resource (e.g., "/cluster0/rack0/node3")
     *  \param status    Reference to receive status
     *  \return          0 on success; -1 on error with errno set:
     *                       EINVAL: invalid parameters, resource path not found,
     *                               or unexpected internal error
     */
    static int get_status (void *h,
                           const std::string &resource_path,
                           resource_pool_t::status_t &status);

    /*! Set resource status (up or down) for the subtree root (typically node) at each ranks in a
     * set. Unknown ranks are silently ignored.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module, it is expected to be a pointer
     *                   to a flux_t object.
     *  \param ranks     RFC 22 idset string representing a set of ranks, or "all".
     *  \param status    resource_pool_t::status_t::UP or resource_pool_t::status_t::DOWN
     *  \return          0 on success; -1 on error with errno set:
     *                       EINVAL: invalid parameters or unexpected internal error
     */
    static int set_rank_status (void *h, std::string_view ranks, resource_pool_t::status_t status);

    /*! Get resource status for the node at a rank.
     *
     *  \param h         Opaque handle. How it is used is an implementation
     *                   detail. However, when it is used within a Flux's
     *                   service module, it is expected to be a pointer
     *                   to a flux_t object.
     *  \param rank      Rank number as a string, e.g. "1"
     *  \param status    Reference to receive status
     *  \return          0 on success; -1 on error with errno set:
     *                       EINVAL: invalid parameters, rank not found,
     *                               or unexpected internal error
     */
    static int get_rank_status (void *h, std::string_view rank, resource_pool_t::status_t &status);
};

}  // namespace resource_model
}  // namespace Flux

#endif  // REAPI_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
