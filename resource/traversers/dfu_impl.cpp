/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

#include "resource/traversers/dfu_impl.hpp"
#include <optional>
#include <boost/iterator/transform_iterator.hpp>
#include <chrono>

using namespace Flux::Jobspec;
using namespace Flux::resource_model;
using namespace Flux::resource_model::detail;

////////////////////////////////////////////////////////////////////////////////
// DFU Traverser Implementation Private API Definitions
////////////////////////////////////////////////////////////////////////////////

const std::string dfu_impl_t::level () const
{
    unsigned int i;
    std::string prefix = "      ";
    for (i = 0; i < m_trav_level; ++i)
        prefix += "---";
    return prefix;
}

void dfu_impl_t::tick ()
{
    m_best_k_cnt++;
    m_color.reset ();
}

bool dfu_impl_t::in_subsystem (edg_t e, subsystem_t subsystem) const
{
    // Short circuit for single subsystem. This will be a common case.
    return (*m_graph)[e].subsystem == subsystem;
}

bool dfu_impl_t::stop_explore (edg_t e, subsystem_t subsystem) const
{
    // Return true if the target vertex has been visited (forward: black)
    // or being visited (cycle: gray).
    vtx_t u = target (e, *m_graph);
    return (m_color.is_gray ((*m_graph)[u].idata.colors[subsystem])
            || m_color.is_black ((*m_graph)[u].idata.colors[subsystem]));
}

bool dfu_impl_t::exclusivity (const std::vector<Jobspec::Resource> &resources, vtx_t u)
{
    // If one of the resources matches with the visiting vertex, u
    // and it requested exclusive access, return true;
    bool exclusive = false;
    for (auto &resource : resources) {
        if (resource_type_t{resource.type} == (*m_graph)[u].type)
            if (resource.exclusive == Jobspec::tristate_t::TRUE)
                exclusive = true;
    }
    return exclusive;
}

int dfu_impl_t::by_avail (const jobmeta_t &meta,
                          subsystem_t s,
                          vtx_t u,
                          const std::vector<Jobspec::Resource> &resources)
{
    int rc = -1;
    int64_t avail = -1;
    planner_t *p = NULL;
    int64_t at = meta.at;
    int saved_errno = errno;
    uint64_t duration = meta.duration;

    errno = 0;
    // Prune by the visiting resource vertex's availability
    // if rack has been allocated exclusively, no reason to descend further.
    p = (*m_graph)[u].schedule.plans;
    if ((avail = planner_avail_resources_during (p, at, duration)) == 0) {
        goto done;
    } else if (avail == -1) {
        m_err_msg += "by_avail: planner_avail_resources_during returned -1.\n";
        if (errno != 0) {
            m_err_msg += strerror (errno);
            m_err_msg += ".\n";
        }
        goto done;
    }
    rc = 0;

done:
    errno = saved_errno;
    return rc;
}

int dfu_impl_t::by_excl (const jobmeta_t &meta,
                         subsystem_t s,
                         vtx_t u,
                         bool exclusive_in,
                         const Jobspec::Resource &resource)
{
    int rc = -1;
    planner_t *p = NULL;
    int64_t at = meta.at;
    int64_t njobs = -1;
    int saved_errno = errno;
    uint64_t duration = meta.duration;

    // If a non-exclusive resource request is explicitly given on a
    // resource that lies under slot, this spec is invalid.
    if (exclusive_in && resource.exclusive == Jobspec::tristate_t::FALSE) {
        errno = EINVAL;
        m_err_msg += "by_excl: exclusivity conflicts at jobspec=";
        m_err_msg += resource.label + " : vertex=" + (*m_graph)[u].name;
        goto done;
    }

    // If a resource request is under slot or an explicit exclusivity is
    // requested, we check the validity of the visiting vertex using
    // its x_checker planner.
    if (exclusive_in || resource.exclusive == Jobspec::tristate_t::TRUE) {
        // If it's exclusive, the traversal type is an allocation, and
        // there are no other allocations on the vertex, then proceed. This
        // check prevents the observed multiple booking issue, where
        // resources with jobs running beyond their walltime can be
        // allocated to another job since the planner considers them
        // available. Note: if Fluxion needs to support shared
        // resources at the leaf level this check will not catch
        // multiple booking.
        if (meta.alloc_type == jobmeta_t::alloc_type_t::AT_ALLOC
            && !(*m_graph)[u].schedule.allocations.empty ())
            goto done;
        errno = 0;
        p = (*m_graph)[u].idata.x_checker;
        njobs = planner_avail_resources_during (p, at, duration);
        if (njobs == -1) {
            m_err_msg += "by_excl: planner_avail_resources_during.\n";
            if (errno != 0) {
                m_err_msg += strerror (errno);
                m_err_msg += ".\n";
            }
            goto restore_errno;
        } else if (njobs < X_CHECKER_NJOBS) {
            goto restore_errno;
        }
    }

    // All cases reached this point indicate further walk is needed.
    rc = 0;

restore_errno:
    errno = saved_errno;
done:
    return rc;
}

int dfu_impl_t::by_subplan (const jobmeta_t &meta,
                            subsystem_t s,
                            vtx_t u,
                            const Jobspec::Resource &resource)
{
    int rc = -1;
    size_t len = 0;
    int64_t at = meta.at;
    uint64_t d = meta.duration;
    std::vector<uint64_t> aggs;
    int saved_errno = errno;
    planner_multi_t *p = (*m_graph)[u].idata.subplans[s];

    if (!p) {
        // Subplan is null if u is a leaf.
        // TODO: handle the unlikely case
        // where the subplan is null for another
        // reason
        rc = 0;
        goto done;
    }
    if (resource.user_data.empty ()) {
        // If user_data is empty, no data is available to prune with.
        rc = 0;
        goto done;
    }
    count_relevant_types (p, resource.user_data, aggs);
    errno = 0;
    len = aggs.size ();
    if ((rc = planner_multi_avail_during (p, at, d, aggs.data (), len)) == -1) {
        if (errno != 0 && errno != ERANGE) {
            m_err_msg += "by_subplan: planner_multi_avail_during returned -1.\n";
            m_err_msg += strerror (errno);
            m_err_msg += ".\n";
        }
        goto done;
    }

done:
    errno = saved_errno;
    return rc;
}

int dfu_impl_t::prune (const jobmeta_t &meta,
                       bool exclusive,
                       subsystem_t s,
                       vtx_t u,
                       const std::vector<Jobspec::Resource> &resources)
{
    int rc = 0;
    // Prune by the visiting resource vertex's availability
    // If resource is not UP, no reason to descend further.
    if (meta.alloc_type != jobmeta_t::alloc_type_t::AT_SATISFIABILITY
        && (*m_graph)[u].status != resource_pool_t::status_t::UP) {
        rc = -1;
        goto done;
    }
    //  RFC 31 constraints only match against type == "node"
    //  unspecified constraint matches everything
    if (meta.constraint != nullptr && (*m_graph)[u].type == node_rt
        && !meta.constraint->match ((*m_graph)[u])) {
        rc = -1;
        goto done;
    }
    // if rack has been allocated exclusively, no reason to descend further.
    if ((rc = by_avail (meta, s, u, resources)) == -1)
        goto done;
    for (auto &resource : resources) {
        if ((*m_graph)[u].type != resource.type && resource.type != slot_rt)
            continue;
        // Prune by exclusivity checker
        if (resource.type != slot_rt && (rc = by_excl (meta, s, u, exclusive, resource)) == -1)
            break;
        // Prune by the subtree planner quantities
        if ((rc = by_subplan (meta, s, u, resource)) == -1)
            break;
    }

done:
    return rc;
}

planner_multi_t *dfu_impl_t::subtree_plan (vtx_t u,
                                           std::vector<uint64_t> &av,
                                           std::vector<const char *> &tp)
{
    size_t len = av.size ();
    int64_t base_time = planner_base_time ((*m_graph)[u].schedule.plans);
    uint64_t duration = planner_duration ((*m_graph)[u].schedule.plans);
    return planner_multi_new (base_time, duration, &av[0], &tp[0], len);
}

int dfu_impl_t::match (vtx_t u,
                       const std::vector<Resource> &resources,
                       const Resource **slot_resource,
                       unsigned int *nslots,
                       const Resource **match_resource)
{
    int rc = -1;
    bool matched = false;
    for (auto &resource : resources) {
        if ((*m_graph)[u].type == resource.type) {
            // Limitations of DFU traverser: jobspec must not
            // have same type at same level Please read utilities/README.md
            if (matched == true)
                goto ret;
            *match_resource = &resource;
            if (!resource.with.empty ()) {
                for (auto &c_resource : resource.with)
                    if (c_resource.type == slot_rt) {
                        *slot_resource = &c_resource;
                        *nslots = m_match->calc_effective_max (c_resource);
                    }
            }
            matched = true;
        } else if (resource.type == slot_rt) {
            // Limitations of DFU traverser: jobspec must not
            // have same type at same level Please read utilities/README.md
            if (matched == true)
                goto ret;
            *slot_resource = &resource;
            *nslots = m_match->calc_effective_max (resource);
            matched = true;
        }
    }
    rc = 0;

ret:
    return rc;
}

bool dfu_impl_t::slot_match (vtx_t u, const Resource *slot_resources)
{
    bool slot_match = true;
    f_out_edg_iterator_t ei, eie;
    if (slot_resources) {
        for (auto &c_resource : (*slot_resources).with) {
            for (tie (ei, eie) = out_edges (u, *m_graph); ei != eie; ++ei) {
                vtx_t tgt = target (*ei, *m_graph);
                if ((*m_graph)[tgt].type == c_resource.type)
                    break;  // found the target resource type of the slot
            }
            if (ei == eie) {
                slot_match = false;
                break;
            }
        }
    } else {
        slot_match = false;
    }
    return slot_match;
}

const std::vector<Resource> &dfu_impl_t::test (vtx_t u,
                                               const std::vector<Resource> &resources,
                                               bool &pristine,
                                               unsigned int &nslots,
                                               match_kind_t &spec)
{
    /* Note on the purpose of pristine: we differentiate two similar but
     * distinct cases with this parameter.
     * Jobspec is allowed to omit the prefix so you can have a spec like
     *    socket[1]->core[2] which will match
     *        cluster[1]->node[1]->socket[2]->core[22].
     * For this case, when you visit the "node" resource vertex, the next
     * Jobspec resource that should be used at the next recursion should
     * be socket[1]. And we enable this if pristine is true.
     * But then once the first match is made, any mismatch afterwards
     * should result in a match failure. For example,
     *    socket[1]->core[2] must fail to match
     *        cluster[1]->socket[1]->numanode[1]->core[22].
     * pristine is used to detect this case.
     */
    bool slot = true;
    const std::vector<Resource> *ret = &resources;
    const Resource *slot_resources = NULL;
    const Resource *match_resources = NULL;
    if (match (u, resources, &slot_resources, &nslots, &match_resources) < 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": siblings in jobspec request same resource type ";
        m_err_msg += ": " + (*m_graph)[u].type + ".\n";
        spec = match_kind_t::NONE_MATCH;
        goto done;
    }
    if ((slot = slot_match (u, slot_resources))) {
        spec = match_kind_t::SLOT_MATCH;
        pristine = false;
        ret = &(slot_resources->with);
    } else if (match_resources) {
        spec = match_kind_t::RESOURCE_MATCH;
        pristine = false;
        ret = &(match_resources->with);
    } else {
        spec = pristine ? match_kind_t::PRISTINE_NONE_MATCH : match_kind_t::NONE_MATCH;
    }

done:
    return *ret;
}

/* Accumulate counts into accum[type] if the type is one of the pruning
 * filter type.
 */
int dfu_impl_t::accum_if (subsystem_t subsystem,
                          resource_type_t type,
                          unsigned int counts,
                          std::map<resource_type_t, int64_t> &accum)
{
    if (m_match->is_pruning_type (subsystem, type)) {
        if (accum.find (type) == accum.end ())
            accum[type] = counts;
        else
            accum[type] += counts;
    }
    return 0;
}

/* Same as above except that accum is unorder_map */
int dfu_impl_t::accum_if (subsystem_t subsystem,
                          resource_type_t type,
                          unsigned int counts,
                          std::unordered_map<resource_type_t, int64_t> &accum)
{
    int rc = -1;
    if (m_match->is_pruning_type (subsystem, type)) {
        if (accum.find (type) == accum.end ())
            accum[type] = counts;
        else
            accum[type] += counts;
        rc = 0;
    }
    return rc;
}

int dfu_impl_t::prime_exp (subsystem_t subsystem, vtx_t u, std::map<resource_type_t, int64_t> &dfv)
{
    int rc = 0;
    f_out_edg_iterator_t ei, ei_end;
    for (tie (ei, ei_end) = out_edges (u, *m_graph); ei != ei_end; ++ei) {
        if (stop_explore (*ei, subsystem) || !in_subsystem (*ei, subsystem))
            continue;
        if ((rc = prime_pruning_filter (subsystem, target (*ei, *m_graph), dfv)) != 0)
            break;
    }
    return rc;
}

int dfu_impl_t::explore_statically (const jobmeta_t &meta,
                                    vtx_t u,
                                    subsystem_t subsystem,
                                    const std::vector<Resource> &resources,
                                    bool pristine,
                                    bool *excl,
                                    visit_t direction,
                                    scoring_api_t &dfu)
{
    int rc = -1;
    int rc2 = -1;
    f_out_edg_iterator_t ei, ei_end;
    for (tie (ei, ei_end) = out_edges (u, *m_graph); ei != ei_end; ++ei) {
        if (stop_explore (*ei, subsystem) || !in_subsystem (*ei, subsystem))
            continue;

        bool x_inout = *excl;
        vtx_t tgt = target (*ei, *m_graph);
        switch (direction) {
            case visit_t::UPV:
                rc = aux_upv (meta, tgt, subsystem, resources, pristine, &x_inout, dfu);
                break;
            case visit_t::DFV:
            default:
                rc = dom_dfv (meta, tgt, resources, pristine, &x_inout, dfu);
                break;
        }
        if (rc == 0) {
            unsigned int count = dfu.avail ();
            eval_edg_t ev_edg (count, count, x_inout, *ei);
            eval_egroup_t egrp (dfu.overall_score (), dfu.avail (), 0, x_inout, false);
            egrp.edges.push_back (ev_edg);
            dfu.add (subsystem, (*m_graph)[tgt].type, egrp);
            rc2 = 0;
        }
    }
    return rc2;
}

bool dfu_impl_t::is_enough (subsystem_t subsystem,
                            const std::vector<Resource> &resources,
                            scoring_api_t &dfu,
                            unsigned int multiplier)
{
    return std::all_of (resources.begin (), resources.end (), [&] (const Resource &resource) {
        unsigned int total = dfu.total_count (subsystem, resource.type);
        unsigned int required = multiplier * m_match->calc_effective_max (resource);
        return total >= required;
    });
}

int dfu_impl_t::new_sat_types (subsystem_t subsystem,
                               const std::vector<Resource> &resources,
                               scoring_api_t &dfu,
                               unsigned int multiplier,
                               std::set<resource_type_t> &sat_types)
{
    for (auto &resource : resources) {
        unsigned int total = dfu.total_count (subsystem, resource.type);
        unsigned int required = multiplier * m_match->calc_effective_max (resource);
        bool sat = total >= required;
        if (sat && sat_types.find (resource.type) == sat_types.end ()) {
            auto ret = sat_types.insert (resource.type);
            if (!ret.second) {
                errno = ENOMEM;
                return -1;
            }
        }
    }
    return 0;
}

int dfu_impl_t::explore_dynamically (const jobmeta_t &meta,
                                     vtx_t u,
                                     subsystem_t subsystem,
                                     const std::vector<Resource> &resources,
                                     bool pristine,
                                     bool *excl,
                                     visit_t direction,
                                     scoring_api_t &dfu,
                                     unsigned int multiplier)
{
    int rc = -1;
    int rc2 = -1;
    auto iter = m_graph_db->metadata.by_outedges.find (u);
    if (iter == m_graph_db->metadata.by_outedges.end ())
        return rc;

    // Once a resource type is sufficiently discovered, not need find more
    std::set<resource_type_t> sat_types;
    // outedges contains outedge map for vertex u, sorted in available resources
    auto &outedges = iter->second;
    for (auto &kv : outedges) {
        edg_t e = kv.second;
        if (stop_explore (e, subsystem) || !in_subsystem (e, subsystem))
            continue;
        vtx_t tgt = target (e, *m_graph);
        if (sat_types.find ((*m_graph)[tgt].type) != sat_types.end ())
            continue;

        bool x_inout = *excl;
        switch (direction) {
            case visit_t::UPV:
                rc = aux_upv (meta, tgt, subsystem, resources, pristine, &x_inout, dfu);
                break;
            case visit_t::DFV:
            default:
                rc = dom_dfv (meta, tgt, resources, pristine, &x_inout, dfu);
                break;
        }
        if (rc == 0) {
            unsigned int count = dfu.avail ();
            eval_edg_t ev_edg (count, count, x_inout, e);
            eval_egroup_t egrp (dfu.overall_score (), dfu.avail (), 0, x_inout, false);
            egrp.edges.push_back (ev_edg);
            dfu.add (subsystem, (*m_graph)[tgt].type, egrp);
            if ((rc2 = new_sat_types (subsystem, resources, dfu, multiplier, sat_types)) < 0)
                break;
            rc2 = 0;
            if (is_enough (subsystem, resources, dfu, multiplier))
                break;
        }
    }
    return rc2;
}

int dfu_impl_t::explore (const jobmeta_t &meta,
                         vtx_t u,
                         subsystem_t subsystem,
                         const std::vector<Resource> &resources,
                         bool pristine,
                         bool *excl,
                         visit_t direction,
                         scoring_api_t &dfu,
                         unsigned int multiplier)
{
    return (!m_match->get_stop_on_k_matches ())
               ? explore_statically (meta, u, subsystem, resources, pristine, excl, direction, dfu)
               : explore_dynamically (meta,
                                      u,
                                      subsystem,
                                      resources,
                                      pristine,
                                      excl,
                                      direction,
                                      dfu,
                                      multiplier);
}

int dfu_impl_t::aux_upv (const jobmeta_t &meta,
                         vtx_t u,
                         subsystem_t aux,
                         const std::vector<Resource> &resources,
                         bool pristine,
                         bool *excl,
                         scoring_api_t &to_parent)
{
    int rc = -1;
    scoring_api_t upv;
    int64_t avail = 0, at = meta.at;
    uint64_t duration = meta.duration;
    planner_t *p = NULL;
    bool x_in = *excl;

    if ((prune (meta, x_in, aux, u, resources) == -1)
        || (m_match->aux_discover_vtx (u, aux, resources, *m_graph)) != 0)
        goto done;

    if (u != (*m_roots)[aux])
        explore (meta, u, aux, resources, pristine, excl, visit_t::UPV, upv);

    p = (*m_graph)[u].schedule.plans;
    if ((avail = planner_avail_resources_during (p, at, duration)) == 0) {
        goto done;
    } else if (avail == -1) {
        m_err_msg += "aux_upv: planner_avail_resources_during returned -1. ";
        m_err_msg += strerror (errno);
        m_err_msg += ".\n";
        goto done;
    }
    if (m_match->aux_finish_vtx (u, aux, resources, *m_graph, upv) != 0)
        goto done;
    if ((rc = resolve (upv, to_parent)) != 0)
        goto done;
done:
    return rc;
}

int dfu_impl_t::dom_exp (const jobmeta_t &meta,
                         vtx_t u,
                         const std::vector<Resource> &resources,
                         bool pristine,
                         bool *excl,
                         scoring_api_t &dfu)
{
    int rc = -1;
    subsystem_t dom = m_match->dom_subsystem ();
    for (auto &s : m_match->subsystems ()) {
        if (s == dom)
            rc = explore (meta, u, s, resources, pristine, excl, visit_t::DFV, dfu);
        else
            rc = explore (meta, u, s, resources, pristine, excl, visit_t::UPV, dfu);
    }
    return rc;
}

int dfu_impl_t::cnt_slot (const std::vector<Resource> &slot_shape, scoring_api_t &dfu_slot)
{
    unsigned int qc = 0;
    unsigned int qg = 0;
    unsigned int fit = 0;
    unsigned int count = 0;
    unsigned int qual_num_slots = UINT_MAX;
    subsystem_t dom = m_match->dom_subsystem ();

    // qualifed slot count is determined by the most constrained resource type
    // both in terms of the amounts available as well as the number of edges into
    // that resource because that represent the match granularity.
    // Say you have 128 units of memory available across two memory resource
    // vertices each with 64 units of memory and you request 1 unit of memory.
    // In this case, you don't have 128 slots available because the match
    // granularity is 64 units. Instead, you have only 2 slots available each
    // with 64 units, and your request will get 1 whole resource vertex.
    qual_num_slots = UINT_MAX;
    for (auto &slot_elem : slot_shape) {
        qc = dfu_slot.qualified_count (dom, slot_elem.type);
        qg = dfu_slot.qualified_granules (dom, slot_elem.type);
        count = m_match->calc_count (slot_elem, qc);
        // constraint check against qualified amounts
        fit = (count == 0) ? count : (qc / count);
        // constraint check against qualified granules
        fit = (fit > qg) ? qg : fit;
        qual_num_slots = (qual_num_slots > fit) ? fit : qual_num_slots;
        dfu_slot.eval_egroups_iter_reset (dom, slot_elem.type);
    }
    return qual_num_slots;
}

int dfu_impl_t::dom_slot (const jobmeta_t &meta,
                          vtx_t u,
                          const std::vector<Resource> &slot_shape,
                          unsigned int nslots,
                          bool pristine,
                          bool *excl,
                          scoring_api_t &dfu)
{
    int rc;
    bool x_inout = true;
    scoring_api_t dfu_slot;
    unsigned int qual_num_slots = 0;
    std::vector<eval_egroup_t> edg_group_vector;
    subsystem_t dom = m_match->dom_subsystem ();

    if ((rc =
             explore (meta, u, dom, slot_shape, pristine, &x_inout, visit_t::DFV, dfu_slot, nslots))
        != 0)
        goto done;
    if ((rc = m_match->dom_finish_slot (dom, dfu_slot)) != 0)
        goto done;

    qual_num_slots = cnt_slot (slot_shape, dfu_slot);
    for (unsigned int i = 0; i < qual_num_slots; ++i) {
        eval_egroup_t edg_group;
        int64_t score = MATCH_MET;
        for (auto &slot_elem : slot_shape) {
            unsigned int j = 0;
            unsigned int qc = dfu_slot.qualified_count (dom, slot_elem.type);
            unsigned int count = m_match->calc_count (slot_elem, qc);
            while (j < count) {
                auto egroup_i = dfu_slot.eval_egroups_iter_next (dom, slot_elem.type);
                if (egroup_i == dfu_slot.eval_egroups_end (dom, slot_elem.type)) {
                    m_err_msg += __FUNCTION__;
                    m_err_msg += ": not enough slots.\n";
                    qual_num_slots = 0;
                    goto done;
                }
                eval_edg_t ev_edg ((*egroup_i).edges[0].count,
                                   (*egroup_i).edges[0].count,
                                   1,
                                   (*egroup_i).edges[0].edge);
                score += (*egroup_i).score;
                edg_group.edges.push_back (ev_edg);
                j += (*egroup_i).edges[0].count;
            }
        }
        edg_group.score = score;
        edg_group.count = 1;
        edg_group.exclusive = 1;
        edg_group_vector.push_back (edg_group);
    }
    for (auto &edg_group : edg_group_vector)
        dfu.add (dom, slot_rt, edg_group);

done:
    return (qual_num_slots) ? 0 : -1;
}

int dfu_impl_t::dom_dfv (const jobmeta_t &meta,
                         vtx_t u,
                         const std::vector<Resource> &resources,
                         bool pristine,
                         bool *excl,
                         scoring_api_t &to_parent)
{
    int rc = -1;
    match_kind_t sm;
    int64_t avail = 0, at = meta.at;
    uint64_t duration = meta.duration;
    bool x_in = *excl || exclusivity (resources, u);
    bool x_inout = x_in;
    bool check_pres = pristine;
    unsigned int nslots = 0;
    scoring_api_t dfu;
    planner_t *p = NULL;
    subsystem_t dom = m_match->dom_subsystem ();
    const std::vector<Resource> &next = test (u, resources, check_pres, nslots, sm);

    m_preorder++;
    if (sm == match_kind_t::NONE_MATCH)
        goto done;
    if ((prune (meta, x_in, dom, u, resources) == -1)
        || (m_match->dom_discover_vtx (u, dom, resources, *m_graph) != 0))
        goto done;
    (*m_graph)[u].idata.colors[dom] = m_color.gray ();
    if (sm == match_kind_t::SLOT_MATCH)
        dom_slot (meta, u, next, nslots, check_pres, &x_inout, dfu);
    else
        dom_exp (meta, u, next, check_pres, &x_inout, dfu);
    *excl = x_in;
    (*m_graph)[u].idata.colors[dom] = m_color.black ();

    p = (*m_graph)[u].schedule.plans;
    if ((avail = planner_avail_resources_during (p, at, duration)) == 0) {
        goto done;
    } else if (avail == -1) {
        m_err_msg += "dom_dfv: planner_avail_resources_during returned -1.\n";
        m_err_msg += strerror (errno);
        m_err_msg += ".\n";
        goto done;
    }
    if (m_match->dom_finish_vtx (u, dom, resources, *m_graph, dfu) != 0)
        goto done;
    if ((rc = resolve (dfu, to_parent)) != 0)
        goto done;
    to_parent.set_avail (avail);
    to_parent.set_overall_score (dfu.overall_score ());

    for (auto &resource : resources) {
        if ((resource.type == (*m_graph)[u].type) && (!resource.label.empty ())) {
            rc = (*m_graph)[u].idata.ephemeral.insert (m_best_k_cnt, "label", resource.label);
            if (rc < 0) {
                m_err_msg += "dom_dfv: inserting label into ephemeral failed.\n";
                m_err_msg += strerror (errno);
                m_err_msg += ".\n";
                goto done;
            }
        }
    }
    m_postorder++;
done:
    return rc;
}

int dfu_impl_t::aux_find_upv (std::shared_ptr<match_writers_t> &writers,
                              const std::string &criteria,
                              vtx_t u,
                              subsystem_t aux,
                              const vtx_predicates_override_t &p)
{
    return 0;
}

int dfu_impl_t::dom_find_dfv (std::shared_ptr<match_writers_t> &w,
                              const std::string &criteria,
                              vtx_t u,
                              const vtx_predicates_override_t &p,
                              const uint64_t jobid,
                              const bool agfilter)
{
    int rc = -1;
    int nchildren = 0;
    f_out_edg_iterator_t ei, ei_end;
    expr_eval_vtx_target_t vtx_target;
    subsystem_t dom = m_match->dom_subsystem ();
    bool result = false;
    bool down = (*m_graph)[u].status == resource_pool_t::status_t::DOWN;
    bool allocated = !(*m_graph)[u].schedule.allocations.empty ();
    bool reserved = !(*m_graph)[u].schedule.reservations.empty ();
    Flux::resource_model::vtx_predicates_override_t p_overridden = p;
    p_overridden.set (down, allocated, reserved);
    std::map<std::string, std::string> agfilter_data;
    planner_multi_t *filter_plan = NULL;

    (*m_graph)[u].idata.colors[dom] = m_color.gray ();
    m_trav_level++;
    for (auto &s : m_match->subsystems ()) {
        for (tie (ei, ei_end) = out_edges (u, *m_graph); ei != ei_end; ++ei) {
            if (stop_explore (*ei, s) || !in_subsystem (*ei, s))
                continue;
            vtx_t tgt = target (*ei, *m_graph);
            rc = (s == dom) ? dom_find_dfv (w, criteria, tgt, p_overridden, jobid, agfilter)
                            : aux_find_upv (w, criteria, tgt, s, p_overridden);
            if (rc > 0) {
                if (w->emit_edg (level (), *m_graph, *ei) < 0) {
                    m_err_msg += __FUNCTION__;
                    m_err_msg += ": emit_edg returned an error.\n";
                }
                nchildren += rc;
            } else if (rc < 0) {
                goto done;
            }
        }
    }
    vtx_target.initialize (p_overridden, m_graph, u);
    (*m_graph)[u].idata.colors[dom] = m_color.black ();

    if ((rc = m_expr_eval.evaluate (criteria, vtx_target, result)) < 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += std::string (": error from evaluate: ") + strerror (errno);
        goto done;
    } else if (!result && !nchildren) {
        goto done;
    }
    if (agfilter) {
        // Check if there's a pruning (aggregate) filter initialized
        if ((filter_plan = (*m_graph)[u].idata.subplans[dom]) == NULL)
            goto done;
        if (jobid == 0) {  // jobid not specified; get totals
            auto now = std::chrono::system_clock::now ();
            int64_t now_epoch =
                std::chrono::duration_cast<std::chrono::seconds> (now.time_since_epoch ()).count ();
            for (size_t i = 0; i < planner_multi_resources_len (filter_plan); ++i) {
                int64_t total_resources = planner_multi_resource_total_at (filter_plan, i);
                int64_t free_now = planner_multi_avail_resources_at (filter_plan, now_epoch, i);
                // Need to check for the case where now = 0, which is used in the testsuite.
                // At will never be 0 in production
                if (free_now == total_resources) {
                    int64_t tmp_usage = planner_multi_avail_resources_at (filter_plan, 0, i);
                    if (tmp_usage != total_resources)
                        free_now = tmp_usage;
                }
                int64_t diff = total_resources - free_now;
                std::string rtype = std::string (planner_multi_resource_type_at (filter_plan, i));
                std::string fcounts =
                    "used:" + std::to_string (diff) + ", total:" + std::to_string (total_resources);
                agfilter_data.insert ({rtype, fcounts});
            }
        } else {  // get agfilter utilization for specified jobid
            auto &job2span = (*m_graph)[u].idata.job2span;
            auto span_it = job2span.find (jobid);
            if (span_it == (*m_graph)[u].idata.job2span.end ()) {
                m_err_msg += __FUNCTION__;
                m_err_msg += ": span missing in job2span ";
                m_err_msg += " for vertex: " + (*m_graph)[u].name + "\n";
                goto done;
            }
            for (size_t i = 0; i < planner_multi_resources_len (filter_plan); ++i) {
                int64_t total_resources = planner_multi_resource_total_at (filter_plan, i);
                int64_t planned_resources =
                    planner_multi_span_planned_at (filter_plan, span_it->second, i);
                std::string rtype = std::string (planner_multi_resource_type_at (filter_plan, i));
                std::string fcounts = "used:" + std::to_string (planned_resources)
                                      + ", total:" + std::to_string (total_resources);
                agfilter_data.insert ({rtype, fcounts});
            }
        }
    }

    // Need to clear out any stale data from the ephemeral object before
    // emitting the vertex, since data could be leftover from previous
    // traversals where the vertex was matched but not emitted
    (*m_graph)[u].idata.ephemeral.check_and_clear_if_stale (m_best_k_cnt);
    if ((rc = w->emit_vtx (level (), *m_graph, u, (*m_graph)[u].size, agfilter_data, true)) < 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += std::string (": error from emit_vtx: ") + strerror (errno);
        goto done;
    }
    // Need to clear out all data from the ephemeral object after
    // emitting the vertex to minimize the amount of stale data
    (*m_graph)[u].idata.ephemeral.clear ();

    rc = nchildren + 1;
done:
    m_trav_level--;
    return rc;
}

int dfu_impl_t::has_root (vtx_t root,
                          std::vector<Resource> &resources,
                          scoring_api_t &dfu,
                          unsigned int *needs)
{
    int rc = 0;
    unsigned int qc;
    unsigned int count;
    *needs = 1;  // if the root is not specified, assume we need 1
    for (auto &resource : resources) {
        if (resource.type == (*m_graph)[root].type) {
            qc = dfu.avail ();
            if ((count = m_match->calc_count (resource, qc)) == 0) {
                rc = -1;
                goto done;
            }
            *needs = count;  // if the root is specified, give that much
        }
    }
done:
    return rc;
}

int dfu_impl_t::has_remaining (vtx_t root, std::vector<Resource> &resources, scoring_api_t &dfu)
{
    int rc = 0;
    for (auto &subsystem : m_match->subsystems ()) {
        std::vector<resource_type_t> types;
        dfu.resrc_types (subsystem, types);
        for (auto &type : types) {
            if (dfu.qualified_count (subsystem, type) == 0) {
                rc = -1;
                goto done;
            }
        }
    }
done:
    return rc;
}

int dfu_impl_t::enforce_constrained (scoring_api_t &dfu)
{
    int rc = 0;
    for (auto subsystem : m_match->subsystems ())
        rc += enforce (subsystem, dfu, true);
    return rc;
}

int dfu_impl_t::resolve_graph (vtx_t root,
                               std::vector<Resource> &resources,
                               scoring_api_t &dfu,
                               bool excl,
                               unsigned int *needs)
{
    int rc = -1;
    subsystem_t dom = m_match->dom_subsystem ();
    if (m_match->dom_finish_graph (dom, resources, *m_graph, dfu) != 0)
        goto done;
    if (has_root (root, resources, dfu, needs) != 0)
        goto done;
    if (has_remaining (root, resources, dfu) != 0)
        goto done;
    if (enforce_constrained (dfu) != 0)
        goto done;
    rc = 0;
done:
    return rc;
}

int dfu_impl_t::resolve (scoring_api_t &dfu, scoring_api_t &to_parent)
{
    int rc = 0;
    if (dfu.overall_score () > MATCH_UNMET) {
        if (dfu.hier_constrain_now ()) {
            for (auto subsystem : m_match->subsystems ())
                rc += enforce (subsystem, dfu);
        } else {
            to_parent.merge (dfu);
        }
    }
    return rc;
}

std::optional<edg_t> find_parent_edge (vtx_t v, resource_graph_t &g, subsystem_t s)
{
    for (auto [ei, e_end] = in_edges (v, g); ei != e_end; ++ei) {
        if (g[*ei].subsystem == s)
            return *ei;
    }
    return std::nullopt;
}

int dfu_impl_t::enforce (subsystem_t subsystem, scoring_api_t &dfu, bool enforce_unconstrained)
{
    int rc = 0;
    try {
        std::vector<resource_type_t> resource_types;
        subsystem_t dom = m_match->dom_subsystem ();
        dfu.resrc_types (subsystem, resource_types);
        for (auto &t : resource_types) {
            if (!dfu.best_k (subsystem, t))
                continue;

            int best_i = dfu.best_i (subsystem, t);
            for (int i = 0; i < best_i; i++) {
                if (dfu.at (subsystem, t, i).root)
                    continue;
                const eval_egroup_t &egroup = dfu.at (subsystem, t, i);
                for (auto &e : egroup.edges) {
                    (*m_graph)[e.edge].idata.set_for_trav_update (e.needs,
                                                                  e.exclusive,
                                                                  m_best_k_cnt);
                    // we need to resolve unconstrained resources up to the root in the dominant
                    // subsystem
                    if (enforce_unconstrained && subsystem == dom) {
                        // there can only be one in-edge in this subsystem, so find it
                        vtx_t parent_v = source (e.edge, *m_graph);
                        std::optional<edg_t> parent_e;
                        // only the root doesn't have one, so natural stop
                        while ((parent_e = find_parent_edge (parent_v, *m_graph, subsystem))) {
                            // if this parent is already marked, all of its will be too
                            if ((*m_graph)[*parent_e].idata.get_trav_token () == m_best_k_cnt) {
                                break;
                            }
                            parent_v = source (*parent_e, *m_graph);
                            vtx_t tgt = target (*parent_e, (*m_graph));
                            bool tgt_constrained = true;
                            if (dfu.is_contained (subsystem, (*m_graph)[tgt].type)) {
                                if (!dfu.best_k (subsystem, (*m_graph)[tgt].type))
                                    tgt_constrained = false;
                            }
                            if (tgt_constrained) {
                                continue;
                            }
                            (*m_graph)[*parent_e].idata.set_for_trav_update ((*m_graph)[tgt].size,
                                                                             false,
                                                                             m_best_k_cnt);
                        }
                    }
                }
            }
        }
    } catch (const std::out_of_range &exception) {
        errno = ERANGE;
        rc = -1;
    }
    return rc;
}

////////////////////////////////////////////////////////////////////////////////
// DFU Traverser Implementation Public API Definitions
////////////////////////////////////////////////////////////////////////////////

dfu_impl_t::dfu_impl_t () = default;
dfu_impl_t::dfu_impl_t (std::shared_ptr<resource_graph_db_t> db, std::shared_ptr<dfu_match_cb_t> m)
    : m_graph_db (db), m_match (m)
{
}
dfu_impl_t::dfu_impl_t (const dfu_impl_t &o) = default;
dfu_impl_t &dfu_impl_t::operator= (const dfu_impl_t &o) = default;
dfu_impl_t::dfu_impl_t (dfu_impl_t &&o) = default;
dfu_impl_t &dfu_impl_t::operator= (dfu_impl_t &&o) = default;
dfu_impl_t::~dfu_impl_t () = default;

const resource_graph_t *dfu_impl_t::get_graph () const
{
    return m_graph;
}

const std::shared_ptr<const resource_graph_db_t> dfu_impl_t::get_graph_db () const
{
    return m_graph_db;
}

const std::shared_ptr<const dfu_match_cb_t> dfu_impl_t::get_match_cb () const
{
    return m_match;
}

const std::string &dfu_impl_t::err_message () const
{
    return m_err_msg;
}

const unsigned int dfu_impl_t::get_preorder_count () const
{
    return m_preorder;
}

const unsigned int dfu_impl_t::get_postorder_count () const
{
    return m_postorder;
}

const std::set<resource_type_t> &dfu_impl_t::get_exclusive_resource_types () const
{
    return m_match->get_exclusive_resource_types ();
}

void dfu_impl_t::set_graph_db (std::shared_ptr<resource_graph_db_t> db)
{
    m_graph_db = db;
    m_graph = &db->resource_graph;
}

void dfu_impl_t::set_match_cb (std::shared_ptr<dfu_match_cb_t> m)
{
    m_match = m;
}

void dfu_impl_t::clear_err_message ()
{
    m_err_msg = "";
}

void dfu_impl_t::reset_color ()
{
    m_color.reset ();
}

int dfu_impl_t::reset_exclusive_resource_types (const std::set<resource_type_t> &x_types)
{
    return m_match->reset_exclusive_resource_types (x_types);
}

int dfu_impl_t::prime_pruning_filter (subsystem_t s,
                                      vtx_t u,
                                      std::map<resource_type_t, int64_t> &to_parent)
{
    int rc = -1;
    int saved_errno = errno;
    std::vector<uint64_t> avail;
    std::vector<const char *> types;
    std::map<resource_type_t, int64_t> dfv;
    resource_type_t type = (*m_graph)[u].type;
    std::vector<resource_type_t> out_prune_types;

    (*m_graph)[u].idata.colors[s] = m_color.gray ();
    accum_if (s, type, (*m_graph)[u].size, to_parent);

    // Don't create and prime a pruning filter if this is
    // a leaf vertex
    if (out_degree (u, *m_graph) == 0) {
        rc = 0;
        goto done;
    }

    if (prime_exp (s, u, dfv) != 0)
        goto done;

    for (auto &aggr : dfv)
        accum_if (s, aggr.first, aggr.second, to_parent);

    if (m_match->get_my_pruning_types (s, (*m_graph)[u].type, out_prune_types)) {
        for (auto &type : out_prune_types) {
            types.push_back (type.c_str ());
            if (dfv.find (type) != dfv.end ())
                avail.push_back (dfv.at (type));
            else
                avail.push_back (0);
        }
    }

    if (avail.empty () || types.empty ()) {
        rc = 0;
        goto done;
    }

    if ((*m_graph)[u].idata.subplans[s] == NULL) {
        errno = 0;
        planner_multi_t *p = NULL;
        if (!(p = subtree_plan (u, avail, types))) {
            m_err_msg += "prime: error initializing a multi-planner. ";
            m_err_msg += strerror (errno);
            goto done;
        }
        (*m_graph)[u].idata.subplans[s] = p;
    } else {
        planner_multi_update ((*m_graph)[u].idata.subplans[s],
                              avail.data (),
                              types.data (),
                              types.size ());
    }
    rc = 0;
done:
    errno = saved_errno;
    (*m_graph)[u].idata.colors[s] = m_color.black ();
    return rc;
}

void dfu_impl_t::prime_jobspec (std::vector<Resource> &resources,
                                std::unordered_map<resource_type_t, int64_t> &to_parent)
{
    subsystem_t subsystem = m_match->dom_subsystem ();
    for (auto &resource : resources) {
        // If the resource is requested as exclusive in the
        // jobspec, add it to the matcher's exclusive resource
        // set. This ensures that the full resource set (which
        // includes shadow resources) is emitted.
        if (resource.exclusive == Jobspec::tristate_t::TRUE)
            m_match->add_exclusive_resource_type (resource.type);
        // Use minimum requirement because you don't want to prune search
        // as far as a subtree satisfies the minimum requirement
        accum_if (subsystem, resource.type, resource.count.min, to_parent);
        prime_jobspec (resource.with, resource.user_data);
        for (auto &aggregate : resource.user_data) {
            accum_if (subsystem, aggregate.first, resource.count.min * aggregate.second, to_parent);
        }
    }
}

int dfu_impl_t::select (Jobspec::Jobspec &j, vtx_t root, jobmeta_t &meta, bool excl)
{
    int rc = -1;
    scoring_api_t dfu;
    bool x_in = excl;
    subsystem_t dom = m_match->dom_subsystem ();

    tick ();
    m_preorder = 0;
    m_postorder = 0;
    rc = dom_dfv (meta, root, j.resources, true, &x_in, dfu);
    if (rc == 0) {
        unsigned int needs = 0;
        eval_edg_t ev_edg (dfu.avail (), dfu.avail (), excl);
        eval_egroup_t egrp (dfu.overall_score (), dfu.avail (), 0, excl, true);
        egrp.edges.push_back (ev_edg);
        dfu.add (dom, (*m_graph)[root].type, egrp);
        rc = resolve_graph (root, j.resources, dfu, excl, &needs);
        m_graph_db->metadata.v_rt_edges[dom].set_for_trav_update (needs, x_in, m_best_k_cnt);
    }
    return rc;
}

int dfu_impl_t::find (std::shared_ptr<match_writers_t> &writers, const std::string &criteria)
{
    int rc = -1;
    vtx_t root;
    expr_eval_vtx_target_t target;
    vtx_predicates_override_t p_overridden;
    bool agfilter = false;
    uint64_t jobid = 0;
    std::vector<std::pair<std::string, std::string>> predicates;

    if (!m_match || !m_graph || !m_graph_db || !writers) {
        errno = EINVAL;
        return rc;
    }
    subsystem_t dom = m_match->dom_subsystem ();
    if (m_graph_db->metadata.roots.find (dom) == m_graph_db->metadata.roots.end ()) {
        errno = EINVAL;
        goto done;
    }
    root = m_graph_db->metadata.roots.at (dom);
    target.initialize (p_overridden, m_graph, root);
    if ((rc = m_expr_eval.validate (criteria, target)) < 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": invalid criteria: " + criteria + ".\n";
        goto done;
    }
    if ((rc = m_expr_eval.extract (criteria, target, predicates)) < 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": failed extraction.\n";
        goto done;
    }
    for (auto const &p : predicates) {
        if (p.first == "jobid-alloc" || p.first == "jobid-span" || p.first == "jobid-tag"
            || p.first == "jobid-reserved") {
            // Don't need try; catch here since validate () succeeded
            jobid = std::stoul (p.second);
        } else if (p.first == "agfilter") {
            if (p.second == "true" || p.second == "t") {
                agfilter = true;
            } else {
                agfilter = false;
            }
        }
    }

    tick ();

    if ((rc = dom_find_dfv (writers, criteria, root, p_overridden, jobid, agfilter)) < 0)
        goto done;

    if (writers->emit_tm (0, 0) == -1) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": emit_tm returned -1.\n";
    }

done:
    return (rc >= 0) ? 0 : -1;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
