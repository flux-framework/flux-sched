/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef DFU_MATCH_MULTILEVEL_ID_IMPL_HPP
#define DFU_MATCH_MULTILEVEL_ID_IMPL_HPP

#include <numeric>
#include "resource/policies/dfu_match_multilevel_id.hpp"

namespace Flux {
namespace resource_model {


/****************************************************************************
 *                                                                          *
 *     MultiLevel ID Match Policy Class's Private Method Definitions        *
 *                                                                          *
 ****************************************************************************/

template<typename FOLD>
multilevel_id_t<FOLD>::score_factor_t::score_factor_t (const std::string &type,
                                                       unsigned add_by,
                                                       unsigned multiple_by)
    : m_type (type), m_add_by (add_by), m_multiply_by (multiple_by)
{

}

template<typename FOLD>
unsigned multilevel_id_t<FOLD>::score_factor_t::calc_factor (
                                                    unsigned base_factor) const
{
    return (base_factor + m_add_by) * m_multiply_by;
}

template<typename FOLD>
unsigned multilevel_id_t<FOLD>::calc_multilevel_scores () const
{
    return std::accumulate (m_multilevel_scores.begin (),
                            m_multilevel_scores.end (), 0);
}


/****************************************************************************
 *                                                                          *
 *      MultiLevel ID Match Policy Class's Public Method Definitions        *
 *                                                                          *
 ****************************************************************************/

template<typename FOLD>
multilevel_id_t<FOLD>::multilevel_id_t ()
{

}

template<typename FOLD>
multilevel_id_t<FOLD>::multilevel_id_t (const std::string &name)
    : dfu_match_cb_t (name)
{

}

template<typename FOLD>
multilevel_id_t<FOLD>::multilevel_id_t (const multilevel_id_t &o)
   : dfu_match_cb_t (o)
{
    m_stop_on_k_matches = o.m_stop_on_k_matches;
    m_multilevel_scores = o.m_multilevel_scores;
    m_multilevel_factors = o.m_multilevel_factors;
}

template<typename FOLD>
multilevel_id_t<FOLD> &multilevel_id_t<FOLD>::operator= (
                                                  const multilevel_id_t &o)
{
    dfu_match_cb_t::operator= (o);
    m_stop_on_k_matches = o.m_stop_on_k_matches;
    m_multilevel_scores = o.m_multilevel_scores;
    m_multilevel_factors = o.m_multilevel_factors;
    return *this;
}

template<typename FOLD>
multilevel_id_t<FOLD>::~multilevel_id_t ()
{

}

template<typename FOLD>
int multilevel_id_t<FOLD>::dom_finish_graph (
                               const subsystem_t &subsystem,
                               const std::vector< 
                                         Flux::Jobspec::Resource> &resources,
                               const f_resource_graph_t &g,
                               scoring_api_t &dfu)
{
    int64_t score = MATCH_MET;
    for (auto &resource : resources) {
        const std::string &type = resource.type;
        unsigned qc = dfu.qualified_count (subsystem, type);
        unsigned count = calc_count (resource, qc);
        if (count == 0) {
            score = MATCH_UNMET;
            break;
        }
        dfu.choose_accum_best_k (subsystem, type, count, m_comp);
    }
    dfu.set_overall_score (score);
    return (score == MATCH_MET)? 0 : -1;
}

template<typename FOLD>
int multilevel_id_t<FOLD>::dom_finish_slot (const subsystem_t &subsystem,
                                            scoring_api_t &dfu)
{
    std::vector<std::string> types;
    dfu.resrc_types (subsystem, types);
    for (auto &type : types)
        dfu.choose_accum_all (subsystem, type, m_comp);
    return 0;
}

template<typename FOLD>
int multilevel_id_t<FOLD>::dom_discover_vtx (
                               vtx_t u,
                               const subsystem_t &subsystem,
                               const std::vector<
                                         Flux::Jobspec::Resource> &resources,
                               const f_resource_graph_t &g)
{
    if (m_multilevel_factors.find (g[u].type) != m_multilevel_factors.end ()) {
        auto &f = m_multilevel_factors[g[u].type];
        m_multilevel_scores.push_back (f.calc_factor (g[u].id));
    }
    incr ();
    return 0;
}

template<typename FOLD>
int multilevel_id_t<FOLD>::dom_finish_vtx (
                               vtx_t u,
                               const subsystem_t &subsystem,
                               const std::vector<
                                         Flux::Jobspec::Resource> &resources,
                               const f_resource_graph_t &g,
                               scoring_api_t &dfu)
{
    int64_t score = MATCH_MET;
    int64_t overall;
    unsigned prefix;

    for (auto &resource : resources) {
        if (resource.type != g[u].type)
            continue;
        // Jobspec resource type matches with the visiting vertex
        for (auto &c_resource : resource.with) {
            // Test children resource count requirements
            const std::string &c_type = c_resource.type;
            unsigned qc = dfu.qualified_count (subsystem, c_type);
            unsigned count = calc_count (c_resource, qc);
            if (count == 0) {
                score = MATCH_UNMET;
                break;
            }
            dfu.choose_accum_best_k (subsystem, c_type, count, m_comp);
        }
    }

    if (m_multilevel_factors.find (g[u].type) != m_multilevel_factors.end ())
        m_multilevel_scores.pop_back ();

    prefix = calc_multilevel_scores ();
    overall = (score == MATCH_MET)? (score + prefix + g[u].id + 1) : score;
    dfu.set_overall_score (overall);
    decr ();
    return (score == MATCH_MET)? 0 : -1;
}

template<typename FOLD>
int multilevel_id_t<FOLD>::set_stop_on_k_matches (unsigned k)
{
    if (k > 1)
        return -1;
    m_stop_on_k_matches = k;
    return 0;
}

template<typename FOLD>
int multilevel_id_t<FOLD>::get_stop_on_k_matches () const
{
    return m_stop_on_k_matches;
}

template<typename FOLD>
int multilevel_id_t<FOLD>::add_score_factor (const std::string &type,
                                             unsigned add_by,
                                             unsigned multiple_by)
{
    try {
        if (m_multilevel_factors.find (type) != m_multilevel_factors.end ()) {
            errno = EEXIST;
            return -1;
        }
        auto ret = m_multilevel_factors.insert (
                       std::pair<std::string, score_factor_t> (
                           type, {type, add_by, multiple_by}));
        if (!ret.second) {
            errno = ENOMEM;
            return -1;
        }
        return 0;
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        return -1;
    }
}

} // resource_model
} // Flux

#endif // DFU_MATCH_MULTILEVEL_ID_IMPL_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
