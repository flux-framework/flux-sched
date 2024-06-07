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
#include "sched_data.hpp"

using namespace Flux::resource_model;

static int test_constructors_and_overload ()
{
    bool bo = false;
    schedule_t *sched1, *sched2, *sched3, *sched4, *sched5 = nullptr;
    uint64_t resource_total = 10;
    const char resource_type[] = "1";

    sched1 = new schedule_t ();
    sched1->allocations[0] = 0;
    sched1->reservations[0] = 0;
    sched1->plans = planner_new (0, 9999, resource_total, resource_type);

    sched2 = new schedule_t ();

    bo = (bo || (*sched1 == *sched2));
    ok (!bo, "initialized schedule_t not equal to uninitialized schedule_t");

    sched2->allocations[0] = 0;
    sched2->reservations[0] = 0;
    sched2->plans = planner_new (0, 9999, resource_total, resource_type);

    bo = (bo || !(*sched1 == *sched2));
    ok (!bo, "initialized schedule_t equal to identically initialized schedule_t");

    sched3 = new schedule_t (*sched1);
    bo = (bo || !(*sched3 == *sched1));
    ok (!bo, "copied schedule_t equal to rhs schedule_t");

    sched4 = new schedule_t ();
    *sched4 = *sched1;
    bo = (bo || !(*sched4 == *sched1));
    ok (!bo, "assigned schedule_t equal to rhs schedule_t");

    sched4->allocations[0] = 1;
    bo = (bo || (*sched4 == *sched1));
    ok (!bo, "mutated schedule_t not equal to original schedule_t");

    sched5 = new schedule_t ();
    sched5->allocations[0] = 0;
    sched5->reservations[0] = 0;
    sched5->plans = planner_new (1, 9999, resource_total, resource_type);
    bo = (bo || (*sched5 == *sched1));
    ok (!bo, "schedule_t initialized with different planner not equal to original schedule_t");

    *sched5 = *sched1;
    bo = (bo || !(*sched5 == *sched1));
    ok (!bo, "schedule_t assignment overload works with state");

    delete sched1;
    delete sched2;
    delete sched3;
    delete sched4;
    delete sched5;

    return 0;
}

int main (int argc, char *argv[])
{
    plan (7);

    test_constructors_and_overload ();

    done_testing ();

    return EXIT_SUCCESS;
}

/*
 * vi: ts=4 sw=4 expandtab
 */
