/*****************************************************************************\
 * Copyright 2024 Lawrence Livermore National Security, LLC
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

#include "planner_internal_tree.hpp"

bool scheduled_point_t::operator== (const scheduled_point_t &o) const
{
    if (point_rb != o.point_rb)
        return false;
    if (resource_rb != o.resource_rb)
        return false;
    if (at != o.at)
        return false;
    if (in_mt_resource_tree != o.in_mt_resource_tree)
        return false;
    if (new_point != o.new_point)
        return false;
    if (ref_count != o.ref_count)
        return false;
    if (remaining != o.remaining)
        return false;
    if (scheduled != o.scheduled)
        return false;

    return true;
}

bool scheduled_point_t::operator!= (const scheduled_point_t &o) const
{
    return !operator== (o);
}

/*
 * vi: ts=4 sw=4 expandtab
 */
