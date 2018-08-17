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

#ifndef DFU_TRAVERSE_IMPL_HPP
#define DFU_TRAVERSE_IMPL_HPP

#include <iostream>
#include <sstream>
#include <cstdlib>
#include <flux/jobspec.hpp>
#include "config/system_defaults.hpp"
#include "schema/resource_data.hpp"
#include "schema/resource_graph.hpp"
#include "policies/base/dfu_match_cb.hpp"
#include "evaluators/scoring_api.hpp"
#include "planner/planner.h"

namespace Flux {
namespace resource_model {
namespace detail {

enum class visit_t { DFV, UPV };

enum class match_kind_t { RESOURCE_MATCH, SLOT_MATCH, NONE_MATCH };

struct jobmeta_t {
    bool allocate = true;
    int64_t jobid = -1;
    int64_t at = -1;
    uint64_t duration = SYSTEM_DEFAULT_DURATION; // will need config ultimately

    void build (Jobspec::Jobspec &jobspec, bool alloc, int64_t id, int64_t t)
    {
        at = t;
        jobid = id;
        allocate = alloc;
        std::string system_key = "system";
        std::string duration_key = "duration";
        auto i = jobspec.attributes.find (system_key);
        if (i->second.find (duration_key) != i->second.end ()) {
            auto j = i->second.find (duration_key);
            duration = (uint64_t)std::atoll (j->second.c_str ());
        }
    }
};

/*! implementation class of dfu_traverser_t
 */
class dfu_impl_t {
public:
    dfu_impl_t ();
    dfu_impl_t (f_resource_graph_t *g, dfu_match_cb_t *m,
                std::map<subsystem_t, vtx_t> *roots);
    dfu_impl_t (const dfu_impl_t &o);
    dfu_impl_t &operator= (const dfu_impl_t &o);
    ~dfu_impl_t ();

    //! Accessors
    const f_resource_graph_t *get_graph () const;
    const std::map<subsystem_t, vtx_t> *get_roots () const;
    const dfu_match_cb_t *get_match_cb () const;
    const std::string &err_message () const;

    void set_graph (f_resource_graph_t *g);
    void set_roots (std::map<subsystem_t, vtx_t> *roots);
    void set_match_cb (dfu_match_cb_t *m);
    void clear_err_message ();

    /*! Exclusive request? Return true if a resource in resources vector
     *  matches resource vertex u and its exclusivity field value is TRUE.
     *  (Note that when the system default configuration is added, it can
     *  return true even if the exclusive field value is UNSPECIFIED
     *  if the system default is configured that way.
     *
     *  \param resources Resource request vector.
     *  \param u         visiting resource vertex.
     *  \return          true or false.
     */
    bool exclusivity (const std::vector<Jobspec::Resource> &resources, vtx_t u);

    /*! Prime the resource graph with subtree plans. The subtree plans are
     *  instantiated on certain resource vertices and updated with the
     *  information on their subtree resources. For example, the subtree plan
     *  of a compute node resource vertex can be configured to track the number
     *  of available compute cores in aggregate at its subtree. dfu_match_cb_t
     *  provides an interface to configure what subtree resources will be tracked
     *  by higher-level resource vertices.
     *
     *  \param subsystem depth-first walk on this subsystem graph for priming.
     *  \param u         visiting resource vertex.
     *  \return          0 on success; -1 on error -- call err_message ()
     *                   for detail.
     */
    int prime (const subsystem_t &subsystem, vtx_t u,
               std::map<std::string, int64_t> &to_parent);

    /*! Prime the resource section of the jobspec. Aggregate configured
     *  subtree resources into jobspec's user_data.  For example,
     *  cluster[1]->rack[2]->node[4]->socket[1]->core[2]
     *  with socket and core types configured to be tracked will be augmented
     *  at the end of priming as:
     *      cluster[1](core:16)->rack[2](core:8)->node[4](core:2)->
     *          socket[1](core:2)->core[2]
     *
     *  The subtree aggregate information is used to prune unnecessary
     *  graph traversals
     *
     *  \param resources Resource request vector.
     *  \param[out] to_parent
     *                   output aggregates on the subtree.
     *  \return          none.
     */
    void prime (std::vector<Jobspec::Resource> &resources,
                std::unordered_map<std::string, int64_t> &to_parent);

    /*! Extract the aggregate info in the lookup object as pertaining to the
     *  planner-tracking resource types into resource_counts array, a form that
     *  can be used with Planner API.
     *
     *  \param plan      multi-planner object.
     *  \param lookup    a map type such as std::map or unordered_map.
     *  \param[out] resource_counts
     *                   output array.
     *  \return          0 on success; -1 on error.
     */
    template <class lookup_t>
    int count (planner_multi_t *plan, const lookup_t &lookup,
               std::vector<uint64_t> &resource_counts);

    /*! Entry point for graph matching and scoring depth-first-and-up (DFU) walk.
     *  It finds best-matching resources and resolves hierarchical constraints.
     *  For example, rack[2]->node[2] will mark the resource graph to select
     *  best-matching two nodes that are spread across two distinct best-matching
     *  racks. What is best matching is defined by the resource selection logic
     *  (derived class of dfu_match_cb_t).
     *
     *  Note that how many resoure vertices have been selected is encoded in the
     *  incoming edge of that vertex for the general case. However, the root
     *  vertex does not have an incoming edge and thus "needs" are passed as
     *  the output from this method to handle the root special case.
     *
     *  \param jobspec   Jobspec object.
     *  \param root      root resource vertex.
     *  \param meta      metadata on this job.
     *  \param exclusive true if exclusive access is requested for root.
     *  \param[out] needs
     *                   number of root resources requested.
     *  \return          0 on success; -1 on error -- call err_message ()
     *                   for detail.
     */
    int select (Jobspec::Jobspec &jobspec, vtx_t root, jobmeta_t &meta,
                bool exclusive, unsigned int *needs);

    /*! Update the resource state based on the previous select invocation
     *  and emit the allocation/reservation information.
     *
     *  \param root      root resource vertex.
     *  \param meta      metadata on the job.
     *  \param needs     the number of root resources requested.
     *  \param excl      exclusive access requested.
     *  \param ss        stringstream into which allocation/reservation
     *                   information is printed.
     *  \return          0 on success; -1 on error -- call err_message ()
     *                   for detail.
     */
    int update (vtx_t root, jobmeta_t &meta, unsigned int needs, bool excl,
                std::stringstream &ss);

    /*! Remove the allocation/reservation referred to by jobid and update
     *  the resource state.
     *
     *  \param root      root resource vertex.
     *  \param jobid     job id.
     *  \return          0 on success; -1 on error.
     */
    int remove (vtx_t root, int64_t jobid);

private:
    const std::string level () const;

    void tick ();
    bool in_subsystem (edg_t e, const subsystem_t &subsystem) const;
    bool stop_explore (edg_t e, const subsystem_t &subsystem) const;

    /*! Various pruning methods
     */
    int by_avail (const jobmeta_t &meta, const std::string &s, vtx_t u,
                  const std::vector<Jobspec::Resource> &resources);
    int by_excl (const jobmeta_t &meta, const std::string &s, vtx_t u,
                 const Jobspec::Resource &resource);
    int by_subplan (const jobmeta_t &meta, const std::string &s, vtx_t u,
                    const Jobspec::Resource &resource);
    int prune (const jobmeta_t &meta, bool excl, const std::string &subsystem,
               vtx_t u, const std::vector<Jobspec::Resource> &resources);

    planner_multi_t *subtree_plan (vtx_t u, std::vector<uint64_t> &avail,
                                   std::vector<const char *> &types);

    /*! Test various matching conditions between jobspec and graph
     * including slot match
     */
    void match (vtx_t u, const std::vector<Jobspec::Resource> &resources,
                const Jobspec::Resource **slot_resource,
                const Jobspec::Resource **match_resource);
    bool slot_match (vtx_t u, const Jobspec::Resource *slot_resource);
    const std::vector<Jobspec::Resource> &test (vtx_t u,
             const std::vector<Jobspec::Resource> &resources, match_kind_t *ko);

    /*! Accumulate count into accum if type matches with one of the resource
     *  types used in the scheduler-driven aggregate update (SDAU) scheme.
     *  dfu_match_cb_t provides an interface to configure what types are used
     *  for SDAU scheme.
     */
    int accum_if (const subsystem_t &subsystem, const std::string &type,
                  unsigned int count, std::map<std::string, int64_t> &accum);
    int accum_if (const subsystem_t &subsystem, const std::string &type,
                  unsigned int count,
                  std::unordered_map<std::string, int64_t> &accum);

    // Explore out-edges for priming the subtree plans
    int prime_exp (const subsystem_t &subsystem,
                   vtx_t u, std::map<std::string, int64_t> &dfv);

    // Explore for resource matching -- only DFV or UPV
    int explore (const jobmeta_t &meta, vtx_t u, const subsystem_t &subsystem,
                 const std::vector<Jobspec::Resource> &resources, bool *excl,
                 visit_t direction, scoring_api_t &to_parent);
    int aux_upv (const jobmeta_t &meta, vtx_t u, const subsystem_t &subsystem,
                 const std::vector<Jobspec::Resource> &resources, bool *excl,
                 scoring_api_t &to_parent);
    int cnt_slot (const std::vector<Jobspec::Resource> &slot_shape,
                  scoring_api_t &dfu_slot);
    int dom_slot (const jobmeta_t &meta, vtx_t u,
                  const std::vector<Jobspec::Resource> &resources, bool *excl,
                  scoring_api_t &dfu);
    int dom_exp (const jobmeta_t &meta, vtx_t u,
                 const std::vector<Jobspec::Resource> &resources, bool *excl,
                 scoring_api_t &to_parent);
    int dom_dfv (const jobmeta_t &meta, vtx_t u,
                 const std::vector<Jobspec::Resource> &resources, bool *excl,
                 scoring_api_t &to_parent);

    // Emit R
    int emit_edge (edg_t e);
    int emit_vertex (vtx_t u, unsigned int needs, bool exclusive,
                     std::stringstream &ss);

    // Update resource graph data store
    int upd_plan (vtx_t u, const subsystem_t &s, unsigned int needs,
                  bool excl, const jobmeta_t &meta, int &n_p,
                  std::map<std::string, int64_t> &to_parent);
    int upd_sched (vtx_t u, const subsystem_t &subsystem, unsigned int needs,
                   bool excl, int n, const jobmeta_t &meta,
                   std::map<std::string, int64_t> &dfu,
                   std::map<std::string, int64_t> &to_parent,
                   std::stringstream &ss);
    int upd_upv (vtx_t u, const subsystem_t &subsystem, unsigned int needs,
                 bool excl, const jobmeta_t &meta,
                 std::map<std::string, int64_t> &to_parent);
    int upd_dfv (vtx_t u, unsigned int needs,
                 bool excl, const jobmeta_t &meta,
                 std::map<std::string, int64_t> &to_parent,
                 std::stringstream &ss);

    // Remove allocation or reservations
    int rem_subtree_plan (vtx_t u, int64_t jobid, const std::string &subsystem);
    int rem_x_checker (vtx_t u, int64_t jobid);
    int rem_plan (vtx_t u, int64_t jobid);
    int rem_dfv (vtx_t u, int64_t jobid);
    int rem_upv (vtx_t u, int64_t jobid);

    // Resolve and enforce hierarchical constraints
    int resolve (vtx_t root, std::vector<Jobspec::Resource> &resources,
                 scoring_api_t &dfu, bool excl, unsigned int *needs);
    int resolve (scoring_api_t &dfu, scoring_api_t &to_parent);
    int enforce (const subsystem_t &subsystem, scoring_api_t &dfu);

    // member data
    color_t m_color;
    uint64_t m_best_k_cnt = 0;
    unsigned int m_trav_level = 0;
    std::map<subsystem_t, vtx_t> *m_roots = NULL;
    f_resource_graph_t *m_graph = NULL;
    dfu_match_cb_t *m_match = NULL;
    std::string m_err_msg = "";
}; // the end of class dfu_impl_t

template <class lookup_t>
int dfu_impl_t::count (planner_multi_t *plan, const lookup_t &lookup,
                       std::vector<uint64_t> &resource_counts)
{
    int rc = 0;
    size_t len = planner_multi_resources_len (plan);
    const char **resource_types = planner_multi_resource_types (plan);
    for (unsigned int i = 0; i < len; ++i) {
        if (lookup.find (resource_types[i]) != lookup.end ()) {
            uint64_t n = (uint64_t)lookup.at (resource_types[i]);
            resource_counts.push_back (n);
        } else {
            resource_counts.push_back (0);
        }
    }
    return rc;
}

} // namespace detail
} // namespace resource_model
} // namespace Flux

#endif // DFU_TRAVERSE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
