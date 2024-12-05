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

#include "resource/evaluators/scoring_api.hpp"

namespace Flux {
namespace resource_model {

////////////////////////////////////////////////////////////////////////////////
// Scoring API Private Method Definitions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Scoring API Public Method Definitions
////////////////////////////////////////////////////////////////////////////////

scoring_api_t::scoring_api_t () = default;
scoring_api_t::scoring_api_t (const scoring_api_t &o) = default;
scoring_api_t::scoring_api_t (scoring_api_t &&o) = default;
scoring_api_t &scoring_api_t::operator= (const scoring_api_t &o) = default;
scoring_api_t &scoring_api_t::operator= (scoring_api_t &&o) = default;
scoring_api_t::~scoring_api_t () = default;

int64_t scoring_api_t::cutline (subsystem_t s, resource_type_t r)
{
    return m_ssys_map[s][r].cutline ();
}

int64_t scoring_api_t::set_cutline (subsystem_t s, resource_type_t r, int64_t c)
{
    return m_ssys_map[s][r].set_cutline (c);
}

void scoring_api_t::eval_egroups_iter_reset (subsystem_t s, resource_type_t r)
{
    m_ssys_map[s][r].eval_egroups_iter_reset ();
}

std::vector<eval_egroup_t>::iterator scoring_api_t::eval_egroups_iter_next (subsystem_t s,
                                                                            resource_type_t r)
{
    return m_ssys_map[s][r].eval_egroups_iter_next ();
}

std::vector<eval_egroup_t>::iterator scoring_api_t::eval_egroups_end (subsystem_t s,
                                                                      resource_type_t r)
{
    return m_ssys_map[s][r].eval_egroups_end ();
}

int scoring_api_t::add (subsystem_t s, resource_type_t r, const eval_egroup_t &eg)
{
    return m_ssys_map[s][r].add (eg);
}

//! Can throw an out_of_range exception
const eval_egroup_t &scoring_api_t::at (subsystem_t s, resource_type_t r, unsigned int i)
{
    return m_ssys_map[s][r].at (i);
}

unsigned int scoring_api_t::qualified_count (subsystem_t s, resource_type_t r)
{
    return m_ssys_map[s][r].qualified_count ();
}

unsigned int scoring_api_t::qualified_granules (subsystem_t s, resource_type_t r)
{
    return m_ssys_map[s][r].qualified_granules ();
}

unsigned int scoring_api_t::total_count (subsystem_t s, resource_type_t r)
{
    return m_ssys_map[s][r].total_count ();
}

unsigned int scoring_api_t::best_k (subsystem_t s, resource_type_t r)
{
    return m_ssys_map[s][r].best_k ();
}

unsigned int scoring_api_t::best_i (subsystem_t s, resource_type_t r)
{
    return m_ssys_map[s][r].best_i ();
}

bool scoring_api_t::hier_constrain_now ()
{
    return m_hier_constrain_now;
}

void scoring_api_t::merge (const scoring_api_t &o)
{
    for (const auto &s : o.m_ssys_map.key_range ()) {
        auto &tmap = o.m_ssys_map.at (s);
        for (auto &[r, ev] : tmap) {
            m_ssys_map[s][r].merge (ev);
        }
    }
}

void scoring_api_t::add_element_to_child_score (int score)
{
    children_score_list.push_back (score);
}
void scoring_api_t::resrc_types (subsystem_t s, std::vector<resource_type_t> &v)
{
    for (auto &kv : m_ssys_map[s])
        v.push_back (kv.first);
}

// overall_score and avail are temporary space such that
// a child vertex visitor can pass the info to the parent vertex
int64_t scoring_api_t::overall_score ()
{
    return m_overall_score;
}

void scoring_api_t::set_overall_score (int64_t overall)
{
    m_overall_score = overall;
}

unsigned int scoring_api_t::avail ()
{
    return m_avail;
}

void scoring_api_t::set_avail (unsigned int avail)
{
    m_avail = avail;
}
void scoring_api_t::set_children_score_vector (const std::vector<int64_t> source)
{
    children_score_list = source;
}
std::vector<int64_t> scoring_api_t::get_children_score_vector ()
{
    return children_score_list;
}
int64_t scoring_api_t::get_children_avearge_score ()
{
    if (children_score_list.size () == 0)
        return 0;
    return static_cast<int64_t> (
        std::accumulate (children_score_list.begin (), children_score_list.end (), 0LL)
        / children_score_list.size ());
}
bool scoring_api_t::is_contained (subsystem_t s, resource_type_t const &r)
{
    return m_ssys_map[s].contains (r);
}

}  // namespace resource_model
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
