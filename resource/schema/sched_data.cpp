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

#include "resource/schema/sched_data.hpp"

namespace Flux {
namespace resource_model {

schedule_t::schedule_t () = default;

schedule_t::schedule_t (const schedule_t &o)
{
    allocations = o.allocations;
    reservations = o.reservations;

    if (plans) {
        if (o.plans) {
            planner_assign (plans, o.plans);
        } else {
            planner_destroy (&plans);
        }
    } else {
        if (o.plans) {
            plans = planner_copy (o.plans);
            if (!plans)
                throw std::runtime_error ("ERROR copying planners\n");
        }
    }
}

schedule_t &schedule_t::operator= (const schedule_t &o)
{
    allocations.clear ();
    reservations.clear ();
    allocations = o.allocations;
    reservations = o.reservations;

    if (plans) {
        if (o.plans) {
            planner_assign (plans, o.plans);
        } else {
            planner_destroy (&plans);
        }
    } else {
        if (o.plans) {
            plans = planner_copy (o.plans);
            if (!plans)
                throw std::runtime_error ("ERROR copying planners\n");
        }
    }
    return *this;
}

bool schedule_t::operator== (const schedule_t &o) const
{
    if (allocations != o.allocations)
        return false;
    if (reservations != o.reservations)
        return false;
    if (!planners_equal (plans, o.plans))
        return false;

    return true;
}

schedule_t::~schedule_t ()
{
    if (plans)
        planner_destroy (&plans);
}

}  // namespace resource_model
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
