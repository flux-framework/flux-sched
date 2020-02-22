/*****************************************************************************\
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

#include "resource/schema/infra_data.hpp"

namespace Flux {
namespace resource_model {


/****************************************************************************
 *                                                                          *
 *   Public Methods on the Data Belonging to the Scheduler Infrastructure   *
 *                                                                          *
 ****************************************************************************/

infra_base_t::infra_base_t ()
{

}

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


/****************************************************************************
 *                                                                          *
 *        Public Methods on Infrastructure Data for Resource Pool           *
 *                                                                          *
 ****************************************************************************/

pool_infra_t::pool_infra_t ()
{

}

pool_infra_t::pool_infra_t (const pool_infra_t &o): infra_base_t (o)
{
    int64_t base_time = 0;
    uint64_t duration = 0;

    // don't copy the content of infrastructure tables and subtree
    // planner objects.
    colors = o.colors;
    for (auto &kv : o.subplans) {
        planner_multi_t *p = kv.second;
        if (!p)
            continue;
        base_time = planner_multi_base_time (p);
        duration = planner_multi_duration (p);
        size_t len = planner_multi_resources_len (p);
        subplans[kv.first] = planner_multi_new (base_time, duration,
                                 planner_multi_resource_totals (p),
                                 planner_multi_resource_types (p), len);
    }
    if (o.x_checker) {
        base_time = planner_base_time (o.x_checker);
        duration = planner_duration (o.x_checker);
        x_checker = planner_new (base_time, duration,
                                 planner_resource_total (o.x_checker),
                                 planner_resource_type (o.x_checker));
    }
}

pool_infra_t &pool_infra_t::operator= (const pool_infra_t &o)
{
    int64_t base_time = 0;
    uint64_t duration = 0;

    // don't copy the content of infrastructure tables and subtree
    // planner objects.
    infra_base_t::operator= (o);
    colors = o.colors;
    for (auto &kv : o.subplans) {
        planner_multi_t *p = kv.second;
        if (!p)
            continue;
        base_time = planner_multi_base_time (p);
        duration = planner_multi_duration (p);
        size_t len = planner_multi_resources_len (p);
        subplans[kv.first] = planner_multi_new (base_time, duration,
                                 planner_multi_resource_totals (p),
                                 planner_multi_resource_types (p), len);
    }
    if (o.x_checker) {
        base_time = planner_base_time (o.x_checker);
        duration = planner_duration (o.x_checker);
        x_checker = planner_new (base_time, duration,
                                 planner_resource_total (o.x_checker),
                                 planner_resource_type (o.x_checker));
    }
    return *this;
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
    job2type.clear ();
    for (auto &kv : subplans)
        planner_multi_destroy (&(kv.second));
    colors.clear ();
    if (x_checker)
        planner_destroy (&x_checker);
}


/****************************************************************************
 *                                                                          *
 *      Public Methods on Infrastructure Data for Resource Relation         *
 *                                                                          *
 ****************************************************************************/

relation_infra_t::relation_infra_t ()
{

}

relation_infra_t::relation_infra_t (const relation_infra_t &o): infra_base_t (o)
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

void relation_infra_t::set_for_trav_update (uint64_t needs, int exclusive,
                                            uint64_t trav_token)
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

} // resource_model
} // Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
