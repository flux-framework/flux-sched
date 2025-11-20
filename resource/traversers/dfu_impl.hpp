/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef DFU_TRAVERSE_IMPL_HPP
#define DFU_TRAVERSE_IMPL_HPP

#include <cstdlib>
#include <cstdint>
#include <memory>
#include "resource/libjobspec/jobspec.hpp"
#include "resource/config/system_defaults.hpp"
#include "resource/schema/resource_data.hpp"
#include "resource/schema/resource_graph.hpp"
#include "resource/policies/base/dfu_match_cb.hpp"
#include "resource/evaluators/scoring_api.hpp"
#include "resource/evaluators/expr_eval_api.hpp"
#include "resource/evaluators/expr_eval_vtx_target.hpp"
#include "resource/writers/match_writers.hpp"
#include "resource/store/resource_graph_store.hpp"
#include "resource/readers/resource_reader_base.hpp"

namespace Flux {
namespace resource_model {
namespace detail {

enum class visit_t { DFV, UPV };

enum class match_kind_t { RESOURCE_MATCH, SLOT_MATCH, NONE_MATCH, PRISTINE_NONE_MATCH };

struct jobmeta_t {
    enum class alloc_type_t : int {
        AT_ALLOC = 0,
        AT_ALLOC_ORELSE_RESERVE = 1,
        AT_SATISFIABILITY = 2
    };

    alloc_type_t alloc_type = alloc_type_t::AT_ALLOC;
    int64_t jobid = -1;
    int64_t at = -1;
    int64_t now = -1;
    uint64_t duration = SYSTEM_DEFAULT_DURATION;  // will need config ultimately
    std::shared_ptr<Jobspec::Constraint> constraint;

    bool is_queue_set () const
    {
        return m_queue_set;
    }

    const std::string &get_queue () const
    {
        return m_queue;
    }

    int build (Jobspec::Jobspec &jobspec,
               alloc_type_t alloc,
               int64_t id,
               int64_t t,
               graph_duration_t &graph_duration)
    {
        at = t;
        now = t;
        jobid = id;
        alloc_type = alloc;
        int64_t g_duration = std::chrono::duration_cast<std::chrono::seconds> (
                                 graph_duration.graph_end - graph_duration.graph_start)
                                 .count ();

        if (g_duration <= 0) {
            errno = EINVAL;
            return -1;
        }
        // Ensure that duration is shorter than expressible
        // int64_t max () for comparison with at in dfu_traverser_t::run
        if ((jobspec.attributes.system.duration > static_cast<double> (g_duration))
            || (jobspec.attributes.system.duration
                > static_cast<double> (std::numeric_limits<int64_t>::max ()))) {
            errno = EINVAL;
            return -1;
        }
        if (jobspec.attributes.system.duration == 0.0f) {
            duration = g_duration;
        } else {
            duration = (int64_t)jobspec.attributes.system.duration;
        }
        if (jobspec.attributes.system.queue != "") {
            m_queue = jobspec.attributes.system.queue;
            m_queue_set = true;
        }
        constraint = jobspec.attributes.system.constraint;
        return 0;
    }

   protected:
    bool m_queue_set = false;
    std::string m_queue = "";
};

/*! implementation class of dfu_traverser_t
 */
class dfu_impl_t {
   public:
    dfu_impl_t ();
    dfu_impl_t (std::shared_ptr<resource_graph_db_t> db, std::shared_ptr<dfu_match_cb_t> m);
    dfu_impl_t (const dfu_impl_t &o);
    dfu_impl_t (dfu_impl_t &&o);
    dfu_impl_t &operator= (const dfu_impl_t &o);
    dfu_impl_t &operator= (dfu_impl_t &&o);
    ~dfu_impl_t ();

    //! Accessors
    const resource_graph_t *get_graph () const;
    const std::shared_ptr<const resource_graph_db_t> get_graph_db () const;
    const std::shared_ptr<const dfu_match_cb_t> get_match_cb () const;
    const std::string &err_message () const;
    const expr_eval_api_t &get_expr_eval () const;
    const unsigned int get_preorder_count () const;
    const unsigned int get_postorder_count () const;
    const std::set<resource_type_t> &get_exclusive_resource_types () const;

    void set_graph_db (std::shared_ptr<resource_graph_db_t> db);
    void set_match_cb (std::shared_ptr<dfu_match_cb_t> m);
    void clear_err_message ();
    void reset_color ();
    int reset_exclusive_resource_types (const std::set<resource_type_t> &x_types);

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
    virtual int prime_pruning_filter (subsystem_t subsystem,
                                      vtx_t u,
                                      std::map<resource_type_t, int64_t> &to_parent);

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
    virtual void prime_jobspec (std::vector<Jobspec::Resource> &resources,
                                std::unordered_map<resource_type_t, int64_t> &to_parent);

    /*! Extract the aggregate info in the lookup object as pertaining to the
     *  planner-tracking resource types into resource_counts array, a form that
     *  can be used with Planner API.
     *
     *  \param plan      multi-planner object.
     *  \param lookup    a map type such as std::map or unordered_map.
     *  \param[out]      resource_counts
     *                   output array.
     *  \return          0 on success; -1 on error.
     */
    template<class lookup_t>
    int count_relevant_types (planner_multi_t *plan,
                              const lookup_t &lookup,
                              std::vector<uint64_t> &resource_counts);

    /*! Entry point for graph matching and scoring depth-first-and-up (DFU) walk.
     *  It finds best-matching resources and resolves hierarchical constraints.
     *  For example, rack[2]->node[2] will mark the resource graph to select
     *  best-matching two nodes that are spread across two distinct best-matching
     *  racks. What is best matching is defined by the resource selection logic
     *  (derived class of dfu_match_cb_t).
     *
     *  Note that how many resource vertices have been selected is encoded in the
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
    int select (Jobspec::Jobspec &jobspec, vtx_t root, jobmeta_t &meta, bool exclusive);

    /*! Traverse the resource graph and emit those resources whose
     *  status is matched with the matching criteria.
     *
     *  \param writers   vertex/edge writers to emit the matched labels
     *  \param criteria  matching criteria expression string. Each
     *                   individual criterion is expressed as a key-value
     *                   pair, representing a predicate p(x) where key is
     *                   is p and value is x.
     *                   Currently supported expressions are
     *                   "status={up|down}", "sched-now={allocated|free}",
     *                   "sched-future={reserved|free}, or any combination
     *                   of them separated with "and", "or", or a whitespace
     *                   which is also interpreted as "and" logical
     *                   operator of two expressions. Parentheses
     *                   are supported to group expressions with a higher
     *                   operator precedence. For example, in "status=up and
     *                   (sched-now=allocated or sched-future=reserved)"
     *                   The parenthesized expression is evaluated
     *                   before taking the "and" operator with the
     *                   the result of the first predicate.
     *                   Other extra whitespaces around predicates are
     *                   ignored.
     *  \return          0 on success; -1 on error.
     */
    int find (std::shared_ptr<match_writers_t> &writers, const std::string &criteria);

    /*! Update the resource state based on the previous select invocation
     *  and emit the allocation/reservation information.
     *
     *  \param root      root resource vertex.
     *  \param writers   vertex/edge writers to emit the matched resources
     *  \param meta      metadata on the job.
     *  \param needs     the number of root resources requested.
     *  \param excl      exclusive access requested.
     *  \return          0 on success; -1 on error -- call err_message ()
     *                   for detail.
     */
    int update (vtx_t root, std::shared_ptr<match_writers_t> &writers, jobmeta_t &meta);

    /*! Update the resource state based on the allocation data (str)
     *  as deserialized by reader and meta.
     *
     *  \param root      root resource vertex.
     *  \param writers   vertex/edge writers to emit the matched resources
     *  \param str       allocation string such as written in JGF.
     *  \param reader    reader object that deserialize str to update the graph
     *  \param meta      metadata on the job.
     *  \return          0 on success; -1 on error -- call err_message ()
     *                   for detail.
     */
    int update (vtx_t root,
                std::shared_ptr<match_writers_t> &writers,
                const std::string &str,
                std::shared_ptr<resource_reader_base_t> &reader,
                jobmeta_t &meta);

    /*! Update to make the resource state ready for the next selection.
     *  Ignore the previous select invocation.
     *
     *  \return          0 on success.
     */
    int update ();

    /*! Remove the allocation/reservation referred to by jobid and update
     *  the resource state.
     *
     *  \param root      root resource vertex.
     *  \param jobid     job id.
     *  \return          0 on success; -1 on error.
     */
    int remove (vtx_t root, int64_t jobid);

    /*! Remove the allocation/reservation referred to by jobid and update
     *  the resource state.
     *
     *  \param root      root resource vertex.
     *  \param to_cancel deallocation string such as written in JGF.
     *  \param reader    reader object that deserialize str to update the graph
     *  \param jobid     job id.
     *  \param full_cancel   bool indicating if the partial cancel cancelled all
     *                       job resources
     *  \return          0 on success; -1 on error.
     */
    int remove (vtx_t root,
                const std::string &to_cancel,
                std::shared_ptr<resource_reader_base_t> &reader,
                int64_t jobid,
                bool &full_cancel);

    /*! Remove the allocation/reservation referred to by jobid and update
     *  the resource state.
     *
     *  \param root      root resource vertex.
     *  \param ranks     job id.
     *  \return          0 on success; -1 on error.
     */
    int remove (vtx_t root, const std::set<int64_t> &ranks);

    /*! Update the resource status to up|down|etc starting at subtree_root.
     *
     *  \param root_path     path to the root of the subtree to update.
     *  \param status        new status value
     *  \return              0 on success; -1 on error.
     *                       EINVAL: graph, roots or match callback not set.
     */
    int mark (const std::string &root_path, resource_pool_t::status_t status);

    /*! Update the resource status to up|down|etc for subgraph
     *  represented by ranks.
     *
     *  \param ranks         set of ranks representing the subgraphs to update.
     *  \param status        new status value
     *  \return              0 on success; -1 on error.
     *                       EINVAL: roots or by_path not found.
     */
    int mark (std::set<int64_t> &ranks, resource_pool_t::status_t status);

    /*! Remove a subgraph rooted at the target path.
     *
     *  \param target  string path to the root of the subgraph to remove
     *  \return              0 on success; -1 on error.
     *                       EINVAL: graph, roots or match callback not set.
     */
    int remove_subgraph (const std::string &target);

    /*! Remove a subgraph corresponding to given ranks.
     *
     *  \param ranks   Set of ranks to remove from the resource graph.
     *  \return              0 on success; -1 on error.
     *                       EINVAL: graph, roots or match callback not set.
     */
    int remove_subgraph (const std::set<int64_t> &ranks);

   protected:
    /************************************************************************
     *                                                                      *
     *                 Protected Match and Util API                           *
     *                                                                      *
     ************************************************************************/
    const std::string level () const;

    void tick ();
    bool in_subsystem (edg_t e, subsystem_t subsystem) const;
    bool stop_explore (edg_t e, subsystem_t subsystem) const;
    bool modify_traversal (vtx_t u, bool emit_shadow_from_parent) const;
    bool stop_explore_best (edg_t e, bool mod) const;
    bool get_eff_exclusive (bool x, bool mod) const;
    unsigned get_eff_needs (unsigned needs, unsigned size, bool mod) const;

    /*! Various pruning methods
     */
    int by_avail (const jobmeta_t &meta,
                  subsystem_t s,
                  vtx_t u,
                  const std::vector<Jobspec::Resource> &resources);
    int by_excl (const jobmeta_t &meta,
                 subsystem_t s,
                 vtx_t u,
                 bool exclusive_in,
                 const Jobspec::Resource &resource);
    int by_subplan (const jobmeta_t &meta,
                    subsystem_t s,
                    vtx_t u,
                    const Jobspec::Resource &resource);
    int by_status (const jobmeta_t &meta, vtx_t u);
    int by_constraint (const jobmeta_t &meta, vtx_t u);
    int prune (const jobmeta_t &meta,
               bool excl,
               subsystem_t subsystem,
               vtx_t u,
               const std::vector<Jobspec::Resource> &resources);

    virtual int prune_resources (const jobmeta_t &meta,
                                 bool excl,
                                 subsystem_t subsystem,
                                 vtx_t u,
                                 const std::vector<Jobspec::Resource> &resources);

    planner_multi_t *subtree_plan (vtx_t u,
                                   std::vector<uint64_t> &avail,
                                   std::vector<const char *> &types);

    /*! Test various matching conditions between jobspec and graph
     * including slot match
     */
    virtual int match (vtx_t u,
                       const std::vector<Jobspec::Resource> &resources,
                       const Jobspec::Resource **slot_resource,
                       unsigned int *nslots,
                       const Jobspec::Resource **match_resource);
    bool slot_match (vtx_t u, const Jobspec::Resource *slot_resource);
    virtual const std::vector<Jobspec::Resource> &test (
        vtx_t u,
        const std::vector<Jobspec::Resource> &resources,
        bool &pristine,
        unsigned int &nslots,
        match_kind_t &ko);
    bool is_pconstraint_matched (vtx_t u, const std::string &property);

    /*! Accumulate count into accum if type matches with one of the resource
     *  types used in the scheduler-driven aggregate update (SDAU) scheme.
     *  dfu_match_cb_t provides an interface to configure what types are used
     *  for SDAU scheme.
     */
    int accum_if (subsystem_t subsystem,
                  resource_type_t type,
                  unsigned int count,
                  std::map<resource_type_t, int64_t> &accum);
    int accum_if (subsystem_t subsystem,
                  resource_type_t type,
                  unsigned int count,
                  std::unordered_map<resource_type_t, int64_t> &accum);

    // Explore out-edges for priming the subtree plans
    int prime_exp (subsystem_t subsystem, vtx_t u, std::map<resource_type_t, int64_t> &dfv);

    // Explore for resource matching -- only DFV or UPV
    int explore (const jobmeta_t &meta,
                 vtx_t u,
                 subsystem_t subsystem,
                 const std::vector<Jobspec::Resource> &resources,
                 bool pristine,
                 bool *excl,
                 visit_t direction,
                 scoring_api_t &dfu,
                 unsigned int multiplier = 1);
    int explore_statically (const jobmeta_t &meta,
                            vtx_t u,
                            subsystem_t subsystem,
                            const std::vector<Jobspec::Resource> &resources,
                            bool pristine,
                            bool *excl,
                            visit_t direction,
                            scoring_api_t &dfu);
    int explore_dynamically (const jobmeta_t &meta,
                             vtx_t u,
                             subsystem_t subsystem,
                             const std::vector<Jobspec::Resource> &resources,
                             bool pristine,
                             bool *excl,
                             visit_t direction,
                             scoring_api_t &dfu,
                             unsigned int multiplier = 1);

    bool is_enough (subsystem_t subsystem,
                    const std::vector<Jobspec::Resource> &resources,
                    scoring_api_t &dfu,
                    unsigned int multiplier);
    int new_sat_types (subsystem_t subsystem,
                       const std::vector<Jobspec::Resource> &resources,
                       scoring_api_t &dfu,
                       unsigned int multiplier,
                       std::set<resource_type_t> &sat_types);
    int aux_upv (const jobmeta_t &meta,
                 vtx_t u,
                 subsystem_t subsystem,
                 const std::vector<Jobspec::Resource> &resources,
                 bool pristine,
                 bool *excl,
                 scoring_api_t &to_parent);
    int cnt_slot (const std::vector<Jobspec::Resource> &slot_shape, scoring_api_t &dfu_slot);
    virtual int dom_slot (const jobmeta_t &meta,
                          vtx_t u,
                          const std::vector<Jobspec::Resource> &resources,
                          unsigned int nslots,
                          bool pristine,
                          bool *excl,
                          scoring_api_t &dfu);
    int dom_exp (const jobmeta_t &meta,
                 vtx_t u,
                 const std::vector<Jobspec::Resource> &resources,
                 bool pristine,
                 bool *excl,
                 scoring_api_t &to_parent);
    int dom_dfv (const jobmeta_t &meta,
                 vtx_t u,
                 const std::vector<Jobspec::Resource> &resources,
                 bool pristine,
                 bool *excl,
                 scoring_api_t &to_parent);
    int dom_find_dfv (std::shared_ptr<match_writers_t> &writers,
                      const std::string &criteria,
                      vtx_t u,
                      const vtx_predicates_override_t &p,
                      const uint64_t jobid,
                      const bool agfilter);
    int aux_find_upv (std::shared_ptr<match_writers_t> &writers,
                      const std::string &criteria,
                      vtx_t u,
                      subsystem_t aux,
                      const vtx_predicates_override_t &p);

    int has_root (vtx_t root,
                  std::vector<Jobspec::Resource> &resources,
                  scoring_api_t &dfu,
                  unsigned int *needs);
    int has_remaining (vtx_t root, std::vector<Jobspec::Resource> &resources, scoring_api_t &dfu);

    // Resolve and enforce hierarchical constraints
    int resolve_graph (vtx_t root,
                       std::vector<Jobspec::Resource> &resources,
                       scoring_api_t &dfu,
                       bool excl,
                       unsigned int *needs);
    int resolve (scoring_api_t &dfu, scoring_api_t &to_parent);
    int enforce (subsystem_t subsystem, scoring_api_t &dfu, bool enforce_unconstrained = false);
    int enforce_constrained (scoring_api_t &dfu);

    /************************************************************************
     *                                                                      *
     *               Protected Update/Emit/Remove API                         *
     *                                                                      *
     ************************************************************************/
    // Emit matched resource set
    int emit_vtx (vtx_t u, std::shared_ptr<match_writers_t> &w, unsigned int needs, bool exclusive);
    int emit_edg (edg_t e, std::shared_ptr<match_writers_t> &w);

    // Update resource graph data store
    int upd_txfilter (vtx_t u,
                      const jobmeta_t &jobmeta,
                      const std::map<resource_type_t, int64_t> &dfu);
    int upd_agfilter (vtx_t u,
                      subsystem_t s,
                      jobmeta_t jobmeta,
                      const std::map<resource_type_t, int64_t> &dfu);
    int upd_idata (vtx_t u,
                   subsystem_t s,
                   jobmeta_t jobmeta,
                   const std::map<resource_type_t, int64_t> &dfu);
    int upd_by_outedges (subsystem_t subsystem, jobmeta_t jobmeta, vtx_t u, edg_t e);
    int upd_plan (vtx_t u,
                  subsystem_t s,
                  unsigned int needs,
                  bool excl,
                  const jobmeta_t &jobmeta,
                  bool full,
                  int &n);
    int accum_to_parent (vtx_t u,
                         subsystem_t s,
                         unsigned int needs,
                         bool excl,
                         const std::map<resource_type_t, int64_t> &dfu,
                         std::map<resource_type_t, int64_t> &to_parent);
    int upd_meta (vtx_t u,
                  subsystem_t s,
                  unsigned int needs,
                  bool excl,
                  int n,
                  const jobmeta_t &jobmeta,
                  const std::map<resource_type_t, int64_t> &dfu,
                  std::map<resource_type_t, int64_t> &to_parent);
    int upd_sched (vtx_t u,
                   std::shared_ptr<match_writers_t> &writers,
                   subsystem_t s,
                   unsigned int needs,
                   bool excl,
                   int n,
                   const jobmeta_t &jobmeta,
                   bool full,
                   const std::map<resource_type_t, int64_t> &dfu,
                   std::map<resource_type_t, int64_t> &to_parent,
                   bool excl_parent);
    int upd_upv (vtx_t u,
                 std::shared_ptr<match_writers_t> &writers,
                 subsystem_t subsystem,
                 unsigned int needs,
                 bool excl,
                 const jobmeta_t &jobmeta,
                 bool full,
                 std::map<resource_type_t, int64_t> &to_parent);
    int upd_dfv (vtx_t u,
                 std::shared_ptr<match_writers_t> &writers,
                 unsigned int needs,
                 bool excl,
                 const jobmeta_t &jobmeta,
                 bool full,
                 std::map<resource_type_t, int64_t> &to_parent,
                 bool emit_shadow,
                 bool excl_parent);
    bool rem_tag (vtx_t u, int64_t jobid);
    int rem_exclusive_filter (vtx_t u, int64_t jobid, const modify_data_t &mod_data);
    int mod_agfilter (vtx_t u,
                      int64_t jobid,
                      subsystem_t s,
                      const modify_data_t &mod_data,
                      bool &stop);
    int mod_idata (vtx_t u,
                   int64_t jobid,
                   subsystem_t s,
                   const modify_data_t &mod_data,
                   bool &stop);
    int mod_plan (vtx_t u, int64_t jobid, modify_data_t &mod_data);
    int mod_upv (vtx_t u, int64_t jobid, const modify_data_t &mod_data);
    int mod_dfv (vtx_t u, int64_t jobid, modify_data_t &mod_data);
    int mod_exv (int64_t jobid, const modify_data_t &mod_data);
    int cancel_vertex (vtx_t vtx, modify_data_t &mod_data, int64_t jobid);
    int clear_vertex (vtx_t vtx, modify_data_t &mod_data);

    // Subgraph removal functions
    int get_subgraph_vertices (vtx_t vtx, std::set<vtx_t> &vtx_set);
    int get_parent_vtx (vtx_t vtx, vtx_t &parent_vtx);
    int remove_metadata_outedges (vtx_t source_vertex, vtx_t dest_vertex);
    void remove_graph_metadata (vtx_t v);
    int remove_subgraph (const std::vector<vtx_t> &roots, std::set<vtx_t> &vertices);

    /************************************************************************
     *                                                                      *
     *                     Protected Member Data                              *
     *                                                                      *
     ************************************************************************/
    color_t m_color;
    uint64_t m_best_k_cnt = 0;
    unsigned int m_trav_level = 0;
    unsigned int m_preorder = 0;
    unsigned int m_postorder = 0;
    resource_graph_t *m_graph = nullptr;
    std::shared_ptr<resource_graph_db_t> m_graph_db = nullptr;
    std::shared_ptr<dfu_match_cb_t> m_match = nullptr;
    expr_eval_api_t m_expr_eval;
    std::string m_err_msg = "";
};  // the end of class dfu_impl_t

template<class lookup_t>
int dfu_impl_t::count_relevant_types (planner_multi_t *plan,
                                      const lookup_t &lookup,
                                      std::vector<uint64_t> &resource_counts)
{
    int rc = 0;
    size_t len = planner_multi_resources_len (plan);
    for (unsigned int i = 0; i < len; ++i) {
        auto type = resource_type_t{planner_multi_resource_type_at (plan, i)};
        if (lookup.find (type) != lookup.end ()) {
            uint64_t n = (uint64_t)lookup.at (type);
            resource_counts.push_back (n);
        } else {
            resource_counts.push_back (0);
        }
    }
    return rc;
}

}  // namespace detail
}  // namespace resource_model
}  // namespace Flux

#endif  // DFU_TRAVERSE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
