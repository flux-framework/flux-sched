/*****************************************************************************\
 * Copyright 2023 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <cstdlib>
#include <sstream>
#include <string>
#include <cstring>
#include <cstdint>
#include <cerrno>
#include <vector>
#include <map>
#include "src/common/libtap/tap.h"
#include "infra_data.hpp"

using namespace Flux::resource_model;

static int test_constructors_and_overload ()
{
    bool bo = false;
    size_t len = 5;
    pool_infra_t *infra1, *infra2, *infra3, *infra4, *infra5 = nullptr;
    pool_infra_t *infra6, *infra7, *infra8, *infra9, *infra10 = nullptr;
    uint64_t resource_total = 100000;
    const char resource_type[] = "core";
    const uint64_t resource_totals[] = {10, 20, 30, 40, 50};
    const char *resource_types[] = {"A", "B", "C", "D", "E"};

    static subsystem_t containment_sub{"containment"};
    static subsystem_t network_sub{"network"};
    static subsystem_t storage_sub{"storage"};
    infra1 = new pool_infra_t ();
    infra1->tags[0] = 0;
    infra1->x_spans[0] = 0;
    infra1->job2span[0] = 0;
    infra1->colors[containment_sub] = 0;
    infra1->colors[network_sub] = 1;
    infra1->x_checker = planner_new (0, INT64_MAX, resource_total, resource_type);
    infra1->subplans[containment_sub] =
        planner_multi_new (0, INT64_MAX, resource_totals, resource_types, len);
    infra1->subplans[network_sub] =
        planner_multi_new (0, INT64_MAX, resource_totals, resource_types, len);

    infra2 = new pool_infra_t ();

    bo = (bo || (*infra1 == *infra2));
    ok (!bo, "initialized pool_infra_t not equal to uninitialized pool_infra_t");

    infra2->tags[0] = 0;
    infra2->x_spans[0] = 0;
    infra2->job2span[0] = 0;
    infra2->colors[containment_sub] = 0;
    infra2->colors[network_sub] = 1;
    infra2->x_checker = planner_new (0, INT64_MAX, resource_total, resource_type);
    infra2->subplans[containment_sub] =
        planner_multi_new (0, INT64_MAX, resource_totals, resource_types, len);
    infra2->subplans[network_sub] =
        planner_multi_new (0, INT64_MAX, resource_totals, resource_types, len);
    bo = (bo || !(*infra1 == *infra2));
    ok (!bo, "initialized pool_infra_t equal to identically initialized pool_infra_t");

    infra3 = new pool_infra_t (*infra1);
    bo = (bo || !(*infra3 == *infra1));
    ok (!bo, "copied pool_infra_t equal to rhs pool_infra_t");

    infra4 = new pool_infra_t ();
    *infra4 = *infra1;
    bo = (bo || !(*infra4 == *infra1));
    ok (!bo, "assigned pool_infra_t equal to rhs pool_infra_t");

    infra4->job2span[0] = 1;
    bo = (bo || (*infra4 == *infra1));
    ok (!bo, "mutated pool_infra_t not equal to original pool_infra_t");

    infra5 = new pool_infra_t ();
    infra5->tags[0] = 0;
    infra5->x_spans[0] = 0;
    infra5->job2span[0] = 0;
    infra5->colors[containment_sub] = 0;
    infra5->colors[network_sub] = 1;
    infra5->x_checker = planner_new (1, INT64_MAX, resource_total, resource_type);
    infra5->subplans[containment_sub] =
        planner_multi_new (0, INT64_MAX, resource_totals, resource_types, len);
    infra5->subplans[network_sub] =
        planner_multi_new (0, INT64_MAX, resource_totals, resource_types, len);
    bo = (bo || (*infra5 == *infra1));
    ok (!bo,
        "pool_infra_t initialized with different x_checker not equal to original pool_infra_t");

    infra6 = new pool_infra_t ();
    infra6->tags[0] = 0;
    infra6->x_spans[0] = 0;
    infra6->job2span[0] = 0;
    infra6->colors[containment_sub] = 0;
    infra6->colors[storage_sub] = 1;
    infra6->x_checker = planner_new (0, INT64_MAX, resource_total, resource_type);
    infra6->subplans[containment_sub] =
        planner_multi_new (0, INT64_MAX, resource_totals, resource_types, len);
    infra6->subplans[network_sub] =
        planner_multi_new (0, INT64_MAX, resource_totals, resource_types, len);
    bo = (bo || (*infra6 == *infra1));
    ok (!bo, "pool_infra_t initialized with different colors not equal to original pool_infra_t");

    infra7 = new pool_infra_t ();
    infra7->tags[0] = 0;
    infra7->x_spans[0] = 0;
    infra7->job2span[0] = 0;
    infra7->colors[containment_sub] = 0;
    infra7->colors[network_sub] = 1;
    infra7->x_checker = planner_new (0, INT64_MAX, resource_total, resource_type);
    infra7->subplans[containment_sub] =
        planner_multi_new (0, INT64_MAX, resource_totals, resource_types, len);
    infra7->subplans[storage_sub] =
        planner_multi_new (0, INT64_MAX, resource_totals, resource_types, len);
    bo = (bo || (*infra7 == *infra1));
    ok (!bo,
        "pool_infra_t initialized with different subplan systems not equal to original "
        "pool_infra_t");

    infra8 = new pool_infra_t ();
    infra8->tags[0] = 0;
    infra8->x_spans[0] = 0;
    infra8->job2span[0] = 0;
    infra8->colors[containment_sub] = 0;
    infra8->colors[network_sub] = 1;
    infra8->x_checker = planner_new (0, INT64_MAX, resource_total, resource_type);
    infra8->subplans[containment_sub] =
        planner_multi_new (0, INT64_MAX, resource_totals, resource_types, len);
    infra8->subplans[network_sub] =
        planner_multi_new (1, INT64_MAX, resource_totals, resource_types, len);
    bo = (bo || (*infra8 == *infra1));
    ok (!bo,
        "pool_infra_t initialized with different subplan planner_multis not equal to original "
        "pool_infra_t");

    infra9 = new pool_infra_t ();
    infra9->tags[0] = 0;
    infra9->x_spans[0] = 0;
    infra9->job2span[0] = 0;
    infra9->colors[containment_sub] = 0;
    infra9->colors[network_sub] = 1;
    infra9->x_checker = planner_new (0, INT64_MAX, resource_total, resource_type);
    infra9->subplans[containment_sub] =
        planner_multi_new (0, INT64_MAX, resource_totals, resource_types, len);
    bo = (bo || (*infra9 == *infra1));
    ok (!bo,
        "pool_infra_t initialized with different subplan planner_multis len not equal to original "
        "pool_infra_t");

    *infra9 = *infra1;
    bo = (bo || !(*infra9 == *infra1));
    ok (!bo, "pool_infra_t assignment overload works with state");

    infra10 = new pool_infra_t (*infra9);
    bo = (bo || !(*infra10 == *infra1));
    ok (!bo, "pool_infra_t ctor works");

    delete infra1;
    delete infra2;
    delete infra3;
    delete infra4;
    delete infra5;
    delete infra6;
    delete infra7;
    delete infra8;
    delete infra9;
    delete infra10;

    return 0;
}

int main (int argc, char *argv[])
{
    plan (12);

    test_constructors_and_overload ();

    done_testing ();

    return EXIT_SUCCESS;
}

/*
 * vi: ts=4 sw=4 expandtab
 */
