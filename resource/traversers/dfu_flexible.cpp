/*****************************************************************************\
 * Copyright 2025 Lawrence Livermore National Security, LLC
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

#include "resource/traversers/dfu_flexible.hpp"

using namespace Flux::Jobspec;
using namespace Flux::resource_model;
using namespace Flux::resource_model::detail;

dfu_flexible_t::dfu_flexible_t () = default;
dfu_flexible_t::dfu_flexible_t (std::shared_ptr<resource_graph_db_t> db,
                                std::shared_ptr<dfu_match_cb_t> m)
    : dfu_impl_t (db, m)
{
}
dfu_flexible_t::dfu_flexible_t (const dfu_flexible_t &o) = default;
dfu_flexible_t &dfu_flexible_t::operator= (const dfu_flexible_t &o) = default;
dfu_flexible_t::dfu_flexible_t (dfu_flexible_t &&o) = default;
dfu_flexible_t &dfu_flexible_t::operator= (dfu_flexible_t &&o) = default;
dfu_flexible_t::~dfu_flexible_t () = default;

int dfu_flexible_t::match (vtx_t u,
                           const std::vector<Resource> &resources,
                           unsigned int *nslots,
                           const Resource **match_resource,
                           const std::vector<Resource> **slot_resources)
{
    int rc = -1;
    bool matched = false, or_matched = false;
    for (const auto &resource : resources) {
        if ((*m_graph)[u].type == resource.type) {
            // Limitations of DFU traverser: jobspec must not
            // have same type at same level Please read utilities/README.md
            if (matched || or_matched)
                goto ret;
            *match_resource = &resource;
            if (!resource.with.empty ()) {
                for (const auto &c_resource : resource.with) {
                    if (c_resource.type == slot_rt) {
                        *slot_resources = &resource.with;
                        *nslots = m_match->calc_effective_max (c_resource);
                    }
                }
            }
            matched = true;
        } else if (resource.type == slot_rt) {
            // Limitations of DFU traverser: jobspec must not
            // have same type at same level except for or_slot.
            if (matched)
                goto ret;
            *slot_resources = &resources;
            // This value is not well defined. In this state, nslots is
            // determined by the last listed or_slot sibling in the jobspec.
            *nslots = m_match->calc_effective_max (resource);
            or_matched = true;
        }
    }
    rc = 0;

ret:
    return rc;
}

const std::vector<Resource> &dfu_flexible_t::test (vtx_t u,
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
    const Resource *match_resources = NULL;
    const std::vector<Resource> *slot_resources = NULL;
    if (match (u, resources, &nslots, &match_resources, &slot_resources) < 0) {
        m_err_msg += __FUNCTION__;
        m_err_msg += ": siblings in jobspec request same resource type ";
        m_err_msg += ": " + (*m_graph)[u].type + ".\n";
        spec = match_kind_t::NONE_MATCH;
        goto done;
    }
    if ((slot_resources)) {
        // set default spec in case no match is found
        spec = pristine ? match_kind_t::PRISTINE_NONE_MATCH : match_kind_t::NONE_MATCH;

        for (Resource r : *slot_resources) {
            if ((slot_match (u, &r))) {
                spec = match_kind_t::SLOT_MATCH;
                pristine = false;
                ret = slot_resources;
            }
        }
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

/* Same as above except that lowest is unorder_map */
int dfu_flexible_t::min_if (subsystem_t subsystem,
                            resource_type_t type,
                            unsigned int counts,
                            std::unordered_map<resource_type_t, int64_t> &lowest)
{
    int rc = -1;
    if (m_match->is_pruning_type (subsystem, type)) {
        if (lowest.find (type) == lowest.end ())
            lowest[type] = counts;
        else if (lowest[type] > counts)
            lowest[type] = counts;
        rc = 0;
    }
    return rc;
}

void dfu_flexible_t::prime_jobspec (std::vector<Resource> &resources,
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

        // Or slots should use a minimum of values rather than an accumulation
        // otherwise possible matches may be filtered out
        if (resource.type == slot_rt) {
            for (const auto &aggregate : resource.user_data) {
                min_if (subsystem,
                        aggregate.first,
                        resource.count.min * aggregate.second,
                        to_parent);
            }
            // If there are Or Slots that do not contain a resource type
            // then we should not consider that resource type for pruning
            for (const auto &aggregate : to_parent) {
                if (resource.user_data.find (aggregate.first) == resource.user_data.end ()) {
                    min_if (subsystem, aggregate.first, 0, to_parent);
                }
            }

        } else {
            for (const auto &aggregate : resource.user_data) {
                accum_if (subsystem,
                          aggregate.first,
                          resource.count.min * aggregate.second,
                          to_parent);
            }
        }
    }
}
std::vector<ResourceList> dfu_flexible_t::split_xor_slots (const ResourceList &resources)
{
    std::vector<ResourceList> results;
    std::vector<ResourceList> results2;
    ResourceList xor_slots;
    ResourceList rest;

    // first separate out xor slots from everything else
    for (auto &resource : resources) {
        if (resource.type == xor_slot_rt)
            xor_slots.push_back (resource);
        else
            rest.push_back (resource);
    }

    // handle case where resource is a parent of an xor slot
    for (auto it = rest.begin (); it != rest.end ();) {
        auto test = dfu_flexible_t::split_xor_slots (it->with);
        if (test.size () > 1) {
            for (const auto &option : test) {
                Resource temp = *it;         // Make a copy of the Resource
                temp.with = option;          // Set the .with member to this option
                results.push_back ({temp});  // Add to results
            }
            it = rest.erase (it);  // Remove from rest, continue
        } else {
            ++it;
        }
    }

    if (results.size () != 0) {
        // one resource is ancestor to an xor slot
        for (auto &result : results) {
            result.insert (result.end (), rest.begin (), rest.end ());
        }
    } else
        // no resources with xor slot ancestry
        // also should be recursive base case
        results = {rest};

    // assume only 1 level of xor slots, no need to recurse
    for (auto &resource : xor_slots) {
        resource.type = slot_rt;
        for (auto result : results) {
            result.push_back (resource);
            results2.push_back (result);
        }
    }

    if (results2.empty ())
        results2 = results;
    return results2;
}

std::tuple<dfu_flexible_t::Key, int, int> dfu_flexible_t::select_or_config (
    const std::vector<Resource> &slots,
    const std::map<resource_type_t, int> &resource_counts,
    unsigned int nslots,
    std::unordered_map<Key, std::tuple<std::map<resource_type_t, int>, int, int>, Hash> &or_config)
{
    int best = -1;
    int i = -1;
    Key index = Key (resource_counts);

    // if available, use precomputed result
    auto const it = or_config.find (index);
    if (it != or_config.end ())
        return it->second;

    // if there is only a single slot, no need to recurse
    // loop through to calculate total number of matches
    if (slots.size () == 1) {
        // start with nslots as upper bound
        int max_matches = nslots;
        const auto &slot = slots[0];

        // find maximum number of matches by taking the min
        // of total matches for each resource in the slot
        for (const auto &slot_elem : slot.with) {
            auto it = resource_counts.find (slot_elem.type);
            unsigned int qc = resource_counts.at (slot_elem.type);
            unsigned int count = m_match->calc_count (slot_elem, qc);
            if (it == resource_counts.end () || it->second < count || count <= 0) {
                max_matches = 0;
                break;
            }
            int possible = m_match->calc_count (slot_elem, it->second) / count;
            max_matches = std::min (max_matches, possible);
        }

        or_config[index] = std::make_tuple (resource_counts, max_matches, 0);
        return or_config[index];
    }

    for (const auto &slot : slots) {
        int test;
        ++i;
        bool match = true;
        std::map<resource_type_t, int> updated_counts = resource_counts;
        // determine if there are enough resources to match with this or_slot
        for (const auto &slot_elem : slot.with) {
            unsigned int qc = resource_counts.at (slot_elem.type);
            unsigned int count = m_match->calc_count (slot_elem, qc);
            if (count <= 0) {
                match = false;
                break;
            }
            updated_counts[slot_elem.type] = updated_counts[slot_elem.type] - count;
        }
        if (!match)
            continue;

        // Store the score returned by select_or_config for updated_counts
        test = std::get<1> (select_or_config (slots, updated_counts, nslots, or_config));
        if (best < test) {
            best = test;
            or_config[index] = std::make_tuple (updated_counts, best + 1, i);
        }
    }

    // if there are no matches, set default score of 0
    // score represents the total number of or_slots that can be scheduled
    // with optimal selection of or_slots
    if (best < 0) {
        std::map<resource_type_t, int> empty;
        or_config[index] = std::make_tuple (empty, best + 1, -1);
    }
    return or_config[index];
}

int dfu_flexible_t::dom_slot (const jobmeta_t &meta,
                              vtx_t u,
                              const std::vector<Resource> &slots,
                              unsigned int nslots,
                              bool pristine,
                              bool *excl,
                              scoring_api_t &dfu)
{
    int rc;
    bool x_inout = true;
    unsigned int qual_num_slots = 0;
    std::vector<eval_egroup_t> edg_group_vector;
    const subsystem_t &dom = m_match->dom_subsystem ();
    std::unordered_set<edg_t *> edges_used;
    scoring_api_t dfu_slot;
    std::unordered_map<Key, std::tuple<std::map<resource_type_t, int>, int, int>, Hash> or_config;
    std::tuple<Key, int, int> current_config;

    // collect a set of all resource types in the or_slots to get resource
    // counts. This does not work well with non leaf vertex resources because
    // it cannot distinguish beyond type. This may be resolvable if graph
    // coloring is removed during the selection process.
    std::vector<Resource> slot_resource_union;
    std::map<resource_type_t, Resource> pre_union;
    std::map<resource_type_t, int> resource_types;
    for (const auto &slot : slots) {
        for (const auto &r : slot.with) {
            auto it = pre_union.find (r.type);
            if (it == pre_union.end ()) {
                resource_types[r.type] = 0;
                pre_union.emplace (r.type, r);
            } else {
                // We need the maximum value for count here
                // otherwise first match policies won't work
                if (it->second.count.min < r.count.min)
                    pre_union.insert_or_assign (r.type, r);
            }
        }
    }

    // Actually add the resources to the vector
    for (const auto &it : pre_union)
        slot_resource_union.push_back (it.second);

    if ((rc = explore (meta,
                       u,
                       dom,
                       slot_resource_union,
                       pristine,
                       &x_inout,
                       visit_t::DFV,
                       dfu_slot,
                       nslots))
        != 0)
        goto done;
    if ((rc = m_match->dom_finish_slot (dom, dfu_slot)) != 0)
        goto done;

    for (auto &it : resource_types) {
        it.second = dfu_slot.qualified_count (dom, it.first);
    }

    // calculate the ideal or_slot config for avail resources.
    // tuple is (key to next best option, current score, index of current best or_slot)
    current_config = select_or_config (slots, resource_types, nslots, or_config);

    qual_num_slots = std::get<1> (current_config);
    for (unsigned int i = 0; i < qual_num_slots; ++i) {
        auto slot_index = std::get<2> (current_config);
        eval_egroup_t edg_group;
        int64_t score = MATCH_MET;

        // use calculated index to determine which or_slot type to use
        for (const auto &slot_elem : slots[slot_index].with) {
            unsigned int j = 0;
            unsigned int qc = dfu_slot.qualified_count (dom, slot_elem.type);
            unsigned int count = m_match->calc_count (slot_elem, qc);
            while (j < count) {
                const auto &egroup_i = dfu_slot.eval_egroups_iter_next (dom, slot_elem.type);
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

        current_config = or_config[std::get<0> (current_config)];
    }
    for (const auto &edg_group : edg_group_vector)
        dfu.add (dom, slot_rt, edg_group);

done:
    return (qual_num_slots) ? 0 : -1;
}

int dfu_flexible_t::prune_resources (const jobmeta_t &meta,
                                     bool exclusive,
                                     subsystem_t s,
                                     vtx_t u,
                                     const std::vector<Jobspec::Resource> &resources)
{
    int rc = 0;
    for (const auto &resource : resources) {
        if ((*m_graph)[u].type != resource.type && resource.type != slot_rt)
            continue;
        // Prune by exclusivity checker
        if (resource.type != slot_rt && (rc = by_excl (meta, s, u, exclusive, resource)) == -1)
            break;
        // Prune by the subtree planner quantities, exclude slots
        if (resource.type != slot_rt && (rc = by_subplan (meta, s, u, resource)) == -1)
            break;
        // Prune by the subtree planner quantities for slots only
        if (resource.type == slot_rt && (rc = by_subplan (meta, s, u, resource)) == -1)
            continue;
    }

done:
    return rc;
}
/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
