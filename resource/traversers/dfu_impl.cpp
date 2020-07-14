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

#include "resource/traversers/dfu_impl.hpp"

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

using namespace Flux::Jobspec;
using namespace Flux::resource_model;
using namespace Flux::resource_model::detail;

/****************************************************************************
 *                                                                          *
 *         DFU Traverser Implementation Private API Definitions             *
 *                                                                          *
 ****************************************************************************/

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

bool dfu_impl_t::in_subsystem (edg_t e, const subsystem_t &subsystem) const
{
    return ((*m_graph)[e].idata.member_of.find (subsystem)
                != (*m_graph)[e].idata.member_of.end ());
}

bool dfu_impl_t::stop_explore (edg_t e, const subsystem_t &subsystem) const
{
    // Return true if the target vertex has been visited (forward: black)
    // or being visited (cycle: gray).
    vtx_t u = target (e, *m_graph);
    return (m_color.is_gray ((*m_graph)[u].idata.colors[subsystem])
            || m_color.is_black ((*m_graph)[u].idata.colors[subsystem]));
}

bool dfu_impl_t::exclusivity (const std::vector<Jobspec::Resource> &resources,
                              vtx_t u)
{
    // If one of the resources matches with the visiting vertex, u
    // and it requested exclusive access, return true;
    bool exclusive = false;
    for (auto &resource: resources) {
        if (resource.type == (*m_graph)[u].type)
            if (resource.exclusive == Jobspec::tristate_t::TRUE)
                exclusive = true;
    }
    return exclusive;
}

int dfu_impl_t::by_avail (const jobmeta_t &meta, const std::string &s, vtx_t u,
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

int dfu_impl_t::by_excl (const jobmeta_t &meta, const std::string &s, vtx_t u,
                         bool exclusive_in, const Jobspec::Resource &resource)
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

    // If a resource request is under slot or an explict exclusivity is
    // requested, we check the validity of the visiting vertex using
    // its x_checker planner.
    if (exclusive_in || resource.exclusive == Jobspec::tristate_t::TRUE) {
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

int dfu_impl_t::by_subplan (const jobmeta_t &meta, const std::string &s, vtx_t u,
                            const Jobspec::Resource &resource)
{
    int rc = -1;
    size_t len = 0;
    int64_t at = meta.at;
    uint64_t d = meta.duration;
    std::vector<uint64_t> aggs;
    int saved_errno = errno;
    planner_multi_t *p = (*m_graph)[u].idata.subplans[s];

    count_relevant_types (p, resource.user_data, aggs);
    if (aggs.empty ()) {
        rc = 0;
        goto done;
    }

    errno = 0;
    len = aggs.size ();
    if ((rc = planner_multi_avail_during (p, at, d, &(aggs[0]), len)) == -1) {
        if (errno != 0) {
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

int dfu_impl_t::prune (const jobmeta_t &meta, bool exclusive,
                       const std::string &s, vtx_t u,
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
    // if rack has been allocated exclusively, no reason to descend further.
    if ( (rc = by_avail (meta, s, u, resources)) == -1)
        goto done;
    for (auto &resource : resources) {
        if ((*m_graph)[u].type != resource.type)
            continue;
        // Prune by exclusivity checker
        if ( (rc = by_excl (meta, s, u, exclusive, resource)) == -1)
            break;
        // Prune by the subtree planner quantities
        if ( (rc = by_subplan (meta, s, u, resource)) == -1)
            break;
    }

done:
    return rc;
}

planner_multi_t *dfu_impl_t::subtree_plan (vtx_t u, std::vector<uint64_t> &av,
                                           std::vector<const char *> &tp)
{
    size_t len = av.size ();
    int64_t base_time = planner_base_time ((*m_graph)[u].schedule.plans);
    uint64_t duration = planner_duration ((*m_graph)[u].schedule.plans);
    return planner_multi_new (base_time, duration, &av[0], &tp[0], len);
}

void dfu_impl_t::match (vtx_t u, const std::vector<Resource> &resources,
                       const Resource **slot_resource,
                       const Resource **match_resource)
{
    for (auto &resource : resources) {
        if ((*m_graph)[u].type == resource.type) {
            *match_resource = &resource;
            if (!resource.with.empty ()) {
                for (auto &c_resource : resource.with)
                    if (c_resource.type == "slot")
                        *slot_resource = &c_resource;
            }
            // Limitations: jobspec must not have same type at same level
            // Please read utilities/README.md
            break;
        } else if (resource.type == "slot") {
            *slot_resource = &resource;
            break;
        }
    }
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
                    break; // found the target resource type of the slot
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
                                 bool &pristine, match_kind_t &spec)
{
    /* Note on the purpose of pristine: we differentiate two similar but
     * distinct cases with this parameter.
     * Jobspec is allowed to omit the prefix so you can have a spec like
     *    socket[1]->core[2] which will match
     *        cluster[1]->node[1]->socket[2]->core[22].
     * For this case, when you visit the "node" resource vertex, the next
     * Jobspec resource that should be used at the next recursion should
     * be socket[1]. And we enable this if pristine is true.
     * But then once the first match is made, any mis-mismatch afterwards
     * should result in a match failure. For example,
     *    socket[1]->core[2] must fail to match
     *        cluster[1]->socket[1]->numanode[1]->core[22].
     * pristine is used to detect this case.
     */
    bool slot = true;
    const std::vector<Resource> *ret = &resources;
    const Resource *slot_resources = NULL;
    const Resource *match_resources = NULL;
    match (u, resources, &slot_resources, &match_resources);
    if ( (slot = slot_match (u, slot_resources))) {
        spec = match_kind_t::SLOT_MATCH;
        pristine = false;
        ret = &(slot_resources->with);
    } else if (match_resources) {
        spec = match_kind_t::RESOURCE_MATCH;
        pristine = false;
        ret = &(match_resources->with);
    } else {
        spec = pristine? match_kind_t::PRESTINE_NONE_MATCH
                       : match_kind_t::NONE_MATCH;
    }
    return *ret;
}

/* Accumulate counts into accum[type] if the type is one of the pruning
 * filter type.
 */
int dfu_impl_t::accum_if (const subsystem_t &subsystem, const std::string &type,
                          unsigned int counts,
                          std::map<std::string, int64_t> &accum)
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
int dfu_impl_t::accum_if (const subsystem_t &subsystem, const std::string &type,
                          unsigned int counts,
                          std::unordered_map<std::string, int64_t> &accum)
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

int dfu_impl_t::prime_exp (const subsystem_t &subsystem, vtx_t u,
                           std::map<std::string, int64_t> &dfv)
{
    int rc = 0;
    f_out_edg_iterator_t ei, ei_end;
    for (tie (ei, ei_end) = out_edges (u, *m_graph); ei != ei_end; ++ei) {
        if (!in_subsystem (*ei, subsystem) || stop_explore (*ei, subsystem))
            continue;
        if ((rc = prime_pruning_filter (subsystem,
                                        target (*ei, *m_graph), dfv)) != 0)
            break;
    }
    return rc;
}

int dfu_impl_t::explore (const jobmeta_t &meta, vtx_t u,
                         const subsystem_t &subsystem,
                         const std::vector<Resource> &resources, bool pristine,
                         bool *excl, visit_t direction, scoring_api_t &dfu)
{
    int rc = -1;
    int rc2 = -1;
    f_out_edg_iterator_t ei, ei_end;
    for (tie (ei, ei_end) = out_edges (u, *m_graph); ei != ei_end; ++ei) {
        if (!in_subsystem (*ei, subsystem) || stop_explore (*ei, subsystem))
            continue;

        bool x_inout = *excl;
        vtx_t tgt = target (*ei, *m_graph);
        switch (direction) {
        case visit_t::UPV:
            rc = aux_upv (meta, tgt, subsystem,
                          resources, pristine, &x_inout, dfu);
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

int dfu_impl_t::aux_upv (const jobmeta_t &meta, vtx_t u, const subsystem_t &aux,
                         const std::vector<Resource> &resources, bool pristine,
                         bool *excl, scoring_api_t &to_parent)
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
    if ( (avail = planner_avail_resources_during (p, at, duration)) == 0) {
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

int dfu_impl_t::dom_exp (const jobmeta_t &meta, vtx_t u,
                         const std::vector<Resource> &resources, bool pristine,
                         bool *excl, scoring_api_t &dfu)
{
    int rc = -1;
    const subsystem_t &dom = m_match->dom_subsystem ();
    for (auto &s : m_match->subsystems ()) {
        if (s == dom)
            rc = explore (meta, u, s, resources,
                          pristine, excl, visit_t::DFV, dfu);
        else
            rc = explore (meta, u, s, resources,
                          pristine, excl, visit_t::UPV, dfu);
    }
    return rc;
}

int dfu_impl_t::cnt_slot (const std::vector<Resource> &slot_shape,
                          scoring_api_t &dfu_slot)
{
    unsigned int qc = 0;
    unsigned int qg = 0;
    unsigned int fit = 0;
    unsigned int count = 0;
    unsigned int qual_num_slots = UINT_MAX;
    const subsystem_t &dom = m_match->dom_subsystem ();

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
        fit = (count == 0)? count : (qc / count);
        // constraint check against qualified granules
        fit = (fit > qg)? qg : fit;
        qual_num_slots = (qual_num_slots > fit)? fit : qual_num_slots;
        dfu_slot.rewind_iter_cur (dom, slot_elem.type);
    }
    return qual_num_slots;
}

int dfu_impl_t::dom_slot (const jobmeta_t &meta, vtx_t u,
                          const std::vector<Resource> &slot_shape, bool pristine,
                          bool *excl, scoring_api_t &dfu)
{
    int rc;
    bool x_inout = true;
    scoring_api_t dfu_slot;
    unsigned int qual_num_slots = 0;
    const subsystem_t &dom = m_match->dom_subsystem ();

    if ( (rc = explore (meta, u, dom, slot_shape, pristine,
                        &x_inout, visit_t::DFV, dfu_slot)) != 0)
        goto done;
    if ((rc = m_match->dom_finish_slot (dom, dfu_slot)) != 0)
        goto done;

    qual_num_slots = cnt_slot (slot_shape, dfu_slot);
    for (unsigned int i = 0; i < qual_num_slots; ++i) {
        eval_egroup_t edg_group;
        int score = MATCH_MET;
        for (auto &slot_elem : slot_shape) {
            unsigned int j = 0;
            unsigned int qc = dfu_slot.qualified_count (dom, slot_elem.type);
            unsigned int count = m_match->calc_count (slot_elem, qc);
            while (j < count) {
                auto egroup_i = dfu_slot.iter_cur (dom, slot_elem.type);
                eval_edg_t ev_edg ((*egroup_i).edges[0].count,
                                   (*egroup_i).edges[0].count, 1,
                                   (*egroup_i).edges[0].edge);
                score += (*egroup_i).score;
                edg_group.edges.push_back (ev_edg);
                j += (*egroup_i).edges[0].count;
                dfu_slot.incr_iter_cur (dom, slot_elem.type);
            }
        }
        edg_group.score = score;
        edg_group.count = 1;
        edg_group.exclusive = 1;
        dfu.add (dom, std::string ("slot"), edg_group);
    }
done:
    return (qual_num_slots)? 0 : -1;
}

int dfu_impl_t::dom_dfv (const jobmeta_t &meta, vtx_t u,
                         const std::vector<Resource> &resources, bool pristine,
                         bool *excl, scoring_api_t &to_parent)
{
    int rc = -1;
    match_kind_t sm;
    int64_t avail = 0, at = meta.at;
    uint64_t duration = meta.duration;
    bool x_in = *excl || exclusivity (resources, u);
    bool x_inout = x_in;
    bool check_pres = pristine;
    scoring_api_t dfu;
    planner_t *p = NULL;
    const std::string &dom = m_match->dom_subsystem ();
    const std::vector<Resource> &next = test (u, resources, check_pres, sm);

    if (sm == match_kind_t::NONE_MATCH)
        goto done;
    if ((prune (meta, x_in, dom, u, resources) == -1)
        || (m_match->dom_discover_vtx (u, dom, resources, *m_graph) != 0))
        goto done;
    (*m_graph)[u].idata.colors[dom] = m_color.gray ();
    if (sm == match_kind_t::SLOT_MATCH)
        dom_slot (meta, u, next, check_pres, &x_inout, dfu);
    else
        dom_exp (meta, u, next, check_pres, &x_inout, dfu);
    *excl = x_in;
    (*m_graph)[u].idata.colors[dom] = m_color.black ();
    p = (*m_graph)[u].schedule.plans;
    if ( (avail = planner_avail_resources_during (p, at, duration)) == 0) {
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
done:
    return rc;
}

int dfu_impl_t::resolve (vtx_t root, std::vector<Resource> &resources,
                         scoring_api_t &dfu, bool excl, unsigned int *needs)
{
    int rc = -1;
    unsigned int qc;
    unsigned int count;
    const subsystem_t &dom = m_match->dom_subsystem ();
    if (m_match->dom_finish_graph (dom, resources, *m_graph, dfu) != 0)
        goto done;

    *needs = 1; // if the root is not specified, assume we need 1
    for (auto &resource : resources) {
        if (resource.type == (*m_graph)[root].type) {
            qc = dfu.avail ();
            if ((count = m_match->calc_count (resource, qc)) == 0)
                goto done;
            *needs = count; // if the root is specified, give that much
        }
    }

    // resolve remaining unconstrained resource types
    for (auto &subsystem : m_match->subsystems ()) {
        std::vector<std::string> types;
        dfu.resrc_types (subsystem, types);
        for (auto &type : types) {
            if (dfu.qualified_count (subsystem, type) == 0)
                goto done;
            else if (!dfu.best_k (subsystem, type))
                dfu.choose_accum_all (subsystem, type);
        }
    }
    rc = 0;
    for (auto subsystem : m_match->subsystems ())
        rc += enforce (subsystem, dfu);
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
        }
        else {
            to_parent.merge (dfu);
        }
    }
    return rc;
}

int dfu_impl_t::enforce (const subsystem_t &subsystem, scoring_api_t &dfu)
{
    int rc = 0;
    try {
        std::vector<std::string> resource_types;
        dfu.resrc_types (subsystem, resource_types);
        for (auto &t : resource_types) {
            int best_i = dfu.best_i (subsystem, t);
            for (int i = 0; i < best_i; i++) {
                if (dfu.at (subsystem, t, i).root)
                    continue;
                const eval_egroup_t &egroup = dfu.at (subsystem, t, i);
                for (auto &e : egroup.edges) {
                    (*m_graph)[e.edge].idata.set_for_trav_update (e.needs,
                                                                  e.exclusive,
                                                                  m_best_k_cnt);
                }
            }
        }
    } catch (const std::out_of_range &exception) {
        errno = ERANGE;
        rc = -1;
    }
    return rc;
}


/****************************************************************************
 *                                                                          *
 *           DFU Traverser Implementation Public API Definitions            *
 *                                                                          *
 ****************************************************************************/

dfu_impl_t::dfu_impl_t ()
{

}

dfu_impl_t::dfu_impl_t (std::shared_ptr<f_resource_graph_t> g,
                        std::shared_ptr<resource_graph_db_t> db,
                        std::shared_ptr<dfu_match_cb_t> m)
    : m_graph (g), m_graph_db (db), m_match (m)
{

}

dfu_impl_t::dfu_impl_t (const dfu_impl_t &o)
{
    m_color = o.m_color;
    m_best_k_cnt = o.m_best_k_cnt;
    m_trav_level = o.m_trav_level;
    m_graph = o.m_graph;
    m_graph_db = o.m_graph_db;
    m_match = o.m_match;
    m_err_msg = o.m_err_msg;
}

dfu_impl_t &dfu_impl_t::operator= (const dfu_impl_t &o)
{
    m_color = o.m_color;
    m_best_k_cnt = o.m_best_k_cnt;
    m_trav_level = o.m_trav_level;
    m_graph = o.m_graph;
    m_graph_db = o.m_graph_db;
    m_match = o.m_match;
    m_err_msg = o.m_err_msg;
    return *this;
}

dfu_impl_t::~dfu_impl_t ()
{

}

const std::shared_ptr<const f_resource_graph_t> dfu_impl_t::get_graph () const
{
    return m_graph;
}

const std::shared_ptr<const
                      resource_graph_db_t> dfu_impl_t::get_graph_db () const
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

void dfu_impl_t::set_graph (std::shared_ptr<f_resource_graph_t> g)
{
    m_graph = g;
}

void dfu_impl_t::set_graph_db (std::shared_ptr<resource_graph_db_t> db)
{
    m_graph_db = db;
}

void dfu_impl_t::set_match_cb (std::shared_ptr<dfu_match_cb_t> m)
{
    m_match = m;
}

void dfu_impl_t::clear_err_message ()
{
    m_err_msg = "";
}

int dfu_impl_t::prime_pruning_filter (const subsystem_t &s, vtx_t u,
                                      std::map<std::string, int64_t> &to_parent)
{
    int rc = -1;
    int saved_errno = errno;
    std::vector<uint64_t> avail;
    std::vector<const char *> types;
    std::map<std::string, int64_t> dfv;
    std::string type = (*m_graph)[u].type;
    std::vector<std::string> out_prune_types;

    (*m_graph)[u].idata.colors[s] = m_color.gray ();
    accum_if (s, type, (*m_graph)[u].size, to_parent);
    if (prime_exp (s, u, dfv) != 0)
        goto done;

    for (auto &aggr : dfv)
        accum_if (s, aggr.first, aggr.second, to_parent);

    if (m_match->get_my_pruning_types (s,
                                       (*m_graph)[u].type, out_prune_types)) {
        for (auto &type : out_prune_types) {
            types.push_back (type.c_str ());
            if (dfv.find (type) != dfv.end ())
                avail.push_back (dfv.at (type));
            else
                avail.push_back (0);
        }
    }

    if (!avail.empty () && !types.empty ()) {
        errno = 0;
        planner_multi_t *p = NULL;
        if (!(p = subtree_plan (u, avail, types)) ) {
            m_err_msg += "prime: error initializing a multi-planner. ";
            m_err_msg += strerror (errno);
            goto done;
        }
        (*m_graph)[u].idata.subplans[s] = p;
    }
    rc = 0;
done:
    errno = saved_errno;
    (*m_graph)[u].idata.colors[s] = m_color.black ();
    return rc;
}

void dfu_impl_t::prime_jobspec (std::vector<Resource> &resources,
                                std::unordered_map<std::string,
                                                   int64_t> &to_parent)
{
    const subsystem_t &subsystem = m_match->dom_subsystem ();
    for (auto &resource : resources) {
        // Use minimum requirement because you don't want to prune search
        // as far as a subtree satisfies the minimum requirement
        accum_if (subsystem, resource.type, resource.count.min, to_parent);
        prime_jobspec (resource.with, resource.user_data);
        for (auto &aggregate : resource.user_data) {
            accum_if (subsystem, aggregate.first,
                      resource.count.min * aggregate.second, to_parent);
        }
    }
}

int dfu_impl_t::select (Jobspec::Jobspec &j, vtx_t root, jobmeta_t &meta,
                        bool excl)
{
    int rc = -1;
    scoring_api_t dfu;
    bool x_in = excl;
    const std::string &dom = m_match->dom_subsystem ();

    tick ();
    rc = dom_dfv (meta, root, j.resources, true, &x_in, dfu);
    if (rc == 0) {
        unsigned int needs = 0;
        eval_edg_t ev_edg (dfu.avail (), dfu.avail (), excl);
        eval_egroup_t egrp (dfu.overall_score (), dfu.avail (), 0, excl, true);
        egrp.edges.push_back (ev_edg);
        dfu.add (dom, (*m_graph)[root].type, egrp);
        rc = resolve (root, j.resources, dfu, excl, &needs);
        m_graph_db->metadata.v_rt_edges[dom].set_for_trav_update (needs, x_in,
                                                                  m_best_k_cnt);
    }
    return rc;
}


/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
