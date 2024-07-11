/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

extern "C" {
#if HAVE_CONFIG_H
#include <config.h>
#endif
}

#include <limits>
#include "resource/schema/infra_data.hpp"

namespace Flux {
namespace resource_model {

////////////////////////////////////////////////////////////////////////////////
// Public Methods on the Data Belonging to the Scheduler Infrastructure
////////////////////////////////////////////////////////////////////////////////

infra_base_t::infra_base_t () = default;

infra_base_t::infra_base_t (const infra_base_t &o)
{
    member_of = o.member_of;
}

infra_base_t &infra_base_t::operator= (const infra_base_t &o)
{
    member_of = o.member_of;
    return *this;
}

infra_base_t::~infra_base_t ()
{
}

////////////////////////////////////////////////////////////////////////////////
// Public Methods on Infrastructure Data for Resource Pool
////////////////////////////////////////////////////////////////////////////////

pool_infra_t::pool_infra_t () = default;

pool_infra_t::pool_infra_t (const pool_infra_t &o) : infra_base_t (o)
{
    ephemeral = o.ephemeral;
    colors = o.colors;
    tags = o.tags;
    x_spans = o.x_spans;
    job2span = o.job2span;

    for (auto &kv : o.subplans) {
        auto sp_it = subplans.find (kv.first);
        if (sp_it != subplans.end ()) {
            // Need to trigger planner_multi destructor
            planner_multi_destroy (&(sp_it->second));
        }
        planner_multi_t *p = kv.second;
        if (!p)
            continue;
        subplans[kv.first] = planner_multi_copy (p);
    }
    if (o.x_checker) {
        if (!x_checker) {
            x_checker = planner_copy (o.x_checker);
        } else {
            planner_assign (x_checker, o.x_checker);
        }
    }
}

pool_infra_t &pool_infra_t::operator= (const pool_infra_t &o)
{
    scrub ();
    subplans.clear ();

    infra_base_t::operator= (o);
    ephemeral = o.ephemeral;
    colors = o.colors;
    tags = o.tags;
    x_spans = o.x_spans;
    job2span = o.job2span;

    for (auto &kv : o.subplans) {
        planner_multi_t *p = kv.second;
        if (!p)
            continue;
        subplans[kv.first] = planner_multi_copy (p);
    }
    if (o.x_checker) {
        if (!x_checker) {
            x_checker = planner_copy (o.x_checker);
        } else {
            planner_assign (x_checker, o.x_checker);
        }
    }
    return *this;
}

bool pool_infra_t::operator== (const pool_infra_t &o) const
{
    if (tags != o.tags)
        return false;
    if (x_spans != o.x_spans)
        return false;
    if (job2span != o.job2span)
        return false;
    if (colors != o.colors)
        return false;
    if (!planners_equal (x_checker, o.x_checker))
        return false;
    if (subplans.size () != o.subplans.size ())
        return false;
    for (auto const &this_it : subplans) {
        auto const other = o.subplans.find (this_it.first);
        if (other == o.subplans.end ())
            return false;
        if (this_it.first != other->first)
            return false;
        if (!planner_multis_equal (this_it.second, other->second))
            return false;
    }

    return true;
}

pool_infra_t::~pool_infra_t ()
{
    for (auto &kv : subplans)
        planner_multi_destroy (&(kv.second));
    if (x_checker)
        planner_destroy (&x_checker);
}

void pool_infra_t::scrub ()
{
    tags.clear ();
    x_spans.clear ();
    job2span.clear ();
    for (auto &kv : subplans)
        planner_multi_destroy (&(kv.second));
    colors.clear ();
    if (x_checker)
        planner_destroy (&x_checker);
    ephemeral.clear ();
}

////////////////////////////////////////////////////////////////////////////////
// Public Methods on Infrastructure Data for Resource Relation
////////////////////////////////////////////////////////////////////////////////

relation_infra_t::relation_infra_t () = default;

relation_infra_t::relation_infra_t (const relation_infra_t &o) : infra_base_t (o)
{
    m_needs = o.m_needs;
    m_trav_token = o.m_trav_token;
    m_exclusive = o.m_exclusive;
}

relation_infra_t &relation_infra_t::operator= (const relation_infra_t &o)
{
    infra_base_t::operator= (o);
    m_needs = o.m_needs;
    m_trav_token = o.m_trav_token;
    m_exclusive = o.m_exclusive;
    return *this;
}

relation_infra_t::~relation_infra_t ()
{
}

void relation_infra_t::scrub ()
{
    m_needs = 0;
    m_trav_token = 0;
    m_exclusive = 0;
}

void relation_infra_t::set_for_trav_update (uint64_t needs, int exclusive, uint64_t trav_token)
{
    m_needs = needs;
    m_trav_token = trav_token;
    m_exclusive = exclusive;
}

uint64_t relation_infra_t::get_needs () const
{
    return m_needs;
}

int relation_infra_t::get_exclusive () const
{
    return m_exclusive;
}

uint64_t relation_infra_t::get_trav_token () const
{
    return m_trav_token;
}

uint64_t relation_infra_t::get_weight () const
{
    return m_weight;
}

void relation_infra_t::set_weight (uint64_t weight)
{
    m_weight = weight;
}

}  // namespace resource_model
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
