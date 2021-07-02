/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#include "resource/schema/sched_data.hpp"

namespace Flux {
namespace resource_model {

schedule_t::schedule_t ()
{

}

schedule_t::schedule_t (const schedule_t &o)
{
    int64_t base_time = 0;
    uint64_t duration = 0;

    // copy constructor does not copy the contents
    // of the schedule tables and of the planner objects.
    if (o.plans) {
        base_time = planner_base_time (o.plans);
        duration = planner_duration (o.plans);
        plans = planner_new (base_time, duration,
                             planner_resource_total (o.plans),
                             planner_resource_type (o.plans));
    }
}

schedule_t &schedule_t::operator= (const schedule_t &o)
{
    int64_t base_time = 0;
    uint64_t duration = 0;

    // assign operator does not copy the contents
    // of the schedule tables and of the planner objects.
    if (o.plans) {
        base_time = planner_base_time (o.plans);
        duration = planner_duration (o.plans);
        plans = planner_new (base_time, duration,
                             planner_resource_total (o.plans),
                             planner_resource_type (o.plans));
    }
    return *this;
}

schedule_t::~schedule_t ()
{
    if (plans)
        planner_destroy (&plans);
}

} // resource_model
} // Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
