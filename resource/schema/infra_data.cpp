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
    // don't copy the content of infrastructure tables and subtree
    // planner objects.
    colors = o.colors;
    for (auto &kv : o.subplans) {
        planner_multi_t *p = kv.second;
        if (!p)
            continue;
        int64_t base_time = planner_multi_base_time (p);
        uint64_t duration = planner_multi_duration (p);
        size_t len = planner_multi_resources_len (p);
        subplans[kv.first] = planner_multi_new (base_time, duration,
                                 planner_multi_resource_totals (p),
                                 planner_multi_resource_types (p), len);
    }
}

pool_infra_t &pool_infra_t::operator= (const pool_infra_t &o)
{
    // don't copy the content of infrastructure tables and subtree
    // planner objects.
    infra_base_t::operator= (o);
    colors = o.colors;
    for (auto &kv : o.subplans) {
        planner_multi_t *p = kv.second;
        if (!p)
            continue;
        int64_t base_time = planner_multi_base_time (p);
        uint64_t duration = planner_multi_duration (p);
        size_t len = planner_multi_resources_len (p);
        subplans[kv.first] = planner_multi_new (base_time, duration,
                                 planner_multi_resource_totals (p),
                                 planner_multi_resource_types (p), len);
    }
    return *this;
}

pool_infra_t::~pool_infra_t ()
{
    for (auto &kv : subplans)
        planner_multi_destroy (&(kv.second));
}

void pool_infra_t::scrub ()
{
    job2span.clear ();
    for (auto &kv : subplans)
        planner_multi_destroy (&(kv.second));
    colors.clear ();
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
    needs = o.needs;
    best_k_cnt = o.best_k_cnt;
    exclusive = o.exclusive;
}

relation_infra_t &relation_infra_t::operator= (const relation_infra_t &o)
{
    infra_base_t::operator= (o);
    needs = o.needs;
    best_k_cnt = o.best_k_cnt;
    exclusive = o.exclusive;
    return *this;
}

relation_infra_t::~relation_infra_t ()
{

}

void relation_infra_t::scrub ()
{
    needs = 0;
    best_k_cnt = 0;
    exclusive = 0;
}


} // resource_model
} // Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
