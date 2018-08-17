/*****************************************************************************\
 *  Copyright (c) 2014 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <cstdlib>
#include <sstream>
#include <string>
#include <cstring>
#include <cstdint>
#include <cerrno>
#include <vector>
#include <map>
#include "planner_multi.h"
#include "src/common/libtap/tap.h"

static void to_stream (int64_t base_time, uint64_t duration, const uint64_t *cnts,
                      const char **types, size_t len, std::stringstream &ss)
{
    if (base_time != -1)
        ss << "B(" << base_time << "):";

    ss << "D(" << duration << "):" << "R(<";
    for (unsigned int i = 0; i < len; ++i)
        ss << types[i] << "(" << cnts[i] << ")";

    ss << ">)";
}

static int test_multi_basics ()
{
    int rc;
    bool bo = false;
    size_t len = 5;
    int64_t t = -1, avail = -1, tmax = INT64_MAX;
    int64_t span1 = -1, span2 = -1, span3 = -1;
    const uint64_t resource_totals[] = {10, 20, 30, 20, 100};
    const char *resource_types[] = {"A", "B", "C", "D", "E"};
    const uint64_t counts1[] = {1, 2, 3, 2, 10};
    const uint64_t counts5[] = {5, 10, 15, 10, 50};
    const uint64_t counts_many_E[] = {1, 1, 1, 1, 50};
    const uint64_t counts_only_C[] = {0, 0, 15, 0, 0};
    const uint64_t *counts10 = resource_totals;
    int64_t avail_resources[] = {0, 0, 0, 0, 0};
    planner_multi_t *ctx = NULL;
    std::stringstream ss;

    errno = 0;
    to_stream (0, tmax, resource_totals, (const char **)resource_types, len, ss);
    ctx = planner_multi_new (0, INT64_MAX, resource_totals, resource_types, len);
    ok ((ctx && !errno), "new with (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 5, counts10, (const char **)resource_types, len, ss);
    rc = planner_multi_avail_during (ctx, 0, 1, counts10, len);
    ok ((!rc && !errno), "checking multi avail works (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 1000, counts5, (const char **)resource_types, len, ss);
    rc = planner_multi_avail_during (ctx, 1, 1000, counts5, len);
    ok ((!rc && !errno), "multi-resource avail works (%s)", ss.str ().c_str ());

    span1 = planner_multi_add_span (ctx, 1, 1000, counts5, len);
    ok ((span1 != -1), "span1 added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 1000, counts10, (const char **)resource_types, len, ss);
    rc = planner_multi_avail_during (ctx, 2000, 1001, counts10, len);
    span2 = planner_multi_add_span (ctx, 2000, 1001, counts10, len);
    ok ((span2 != -1), "span2 added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 2990, counts1, (const char **)resource_types, len, ss);
    rc = planner_multi_avail_during (ctx, 10, 2991, counts1, len);
    ok ((rc == -1) && !errno, "over-alloc multi: (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 1990, counts1, (const char **)resource_types, len, ss);
    rc = planner_multi_avail_during (ctx, 10, 1990, counts1, len);
    ok ((!rc && !errno), "overlapped multi resources (%s)", ss.str ().c_str ());
    span3 = planner_multi_add_span (ctx, 10, 1990, counts1, len);
    ok ((span3 != -1), "span3 added for (%s)", ss.str ().c_str ());

    ss.str ("");

    avail = planner_multi_avail_resources_at (ctx, 1, 0);
    bo = (bo || avail != 5);
    avail = planner_multi_avail_resources_at (ctx, 10, 1);
    bo = (bo || avail != 8);
    planner_multi_avail_resources_array_at (ctx, 500, avail_resources, len);
    bo = (bo || avail_resources[2] != 12);
    avail = planner_multi_avail_resources_at (ctx, 1500, 0);
    bo = (bo || avail != 9);
    avail = planner_multi_avail_resources_at (ctx, 2000, 0);
    bo = (bo || avail != 0);
    avail = planner_multi_avail_resources_at (ctx, 2500, 0);
    bo = (bo || avail != 0);
    avail = planner_multi_avail_resources_at (ctx, 3000, 4);
    bo = (bo || avail != 0);
    avail = planner_multi_avail_resources_at (ctx, 3001, 2);
    bo = (bo || avail != 30);
    ok (!bo && !errno, "avail_at_resources_* works");

    bo = false;
    t = planner_multi_avail_time_first (ctx, 0, 9, counts_only_C, len);
    bo = (bo || t != 0);
    t = planner_multi_avail_time_next (ctx);
    bo = (bo || t != 1);
    t = planner_multi_avail_time_next (ctx);
    bo = (bo || t != 1001);
    t = planner_multi_avail_time_next (ctx);
    bo = (bo || t != 3001);
    t = planner_multi_avail_time_next (ctx);
    bo = (bo || t != -1);
    ok (!bo && errno == ENOENT, "avail_time_* works");

    errno = 0;
    bo = false;
    t = planner_multi_avail_time_first (ctx, 0, 10, counts_many_E, len);
    bo = (bo || t != 0);
    t = planner_multi_avail_time_next (ctx);
    bo = (bo || t != 1001);
    t = planner_multi_avail_time_next (ctx);
    bo = (bo || t != 3001);
    t = planner_multi_avail_time_next (ctx);
    bo = (bo || t != -1);
    ok (!bo && errno == ENOENT, "avail_time_* test2 works");

    planner_multi_destroy (&ctx);
    return 0;
}

static int test_multi_getters ()
{
    bool bo = false;
    size_t len = 2;
    int64_t rc = -1;
    int64_t avail = -1;
    std::stringstream ss;
    planner_multi_t *ctx = NULL;
    uint64_t resource_total[] = {10, 20};
    const char *resource_types[] = {"1", "2"};

    errno = 0;
    to_stream (0, 9999, resource_total, (const char **)resource_types, len, ss);
    ctx = planner_multi_new (0, 9999, resource_total, resource_types, len);
    ok ((ctx && !errno), "new with (%s)", ss.str ().c_str ());

    rc = planner_multi_base_time (ctx);
    ok ((rc == 0), "base_time works for (%s)", ss.str ().c_str ());

    rc = planner_multi_duration (ctx);
    ok ((rc == 9999), "duration works for (%s)", ss.str ().c_str ());

    avail = planner_multi_resource_total_at (ctx, 0);
    bo = (bo || (avail != 10));

    avail = planner_multi_resource_total_by_type (ctx, "2");
    bo = (bo || (avail != 20));

    ok ((!bo && !errno), "planner_multi getters work");

    planner_multi_destroy (&ctx);
    return 0;
}

static int test_multi_strictly_larger ()
{
    size_t len = 5;
    int rc = -1;
    int64_t t = -1;
    int64_t tmax = INT64_MAX;
    int64_t span = -1;
    const uint64_t resource_totals[] = {10, 20, 30, 40, 50};
    const char *resource_types[] = {"A", "B", "C", "D", "E"};
    const uint64_t request1[] = {1, 0, 0, 0, 0};
    const uint64_t request2[] = {0, 2, 0, 0, 0};
    const uint64_t request3[] = {0, 0, 3, 0, 0};
    const uint64_t request4[] = {0, 0, 0, 4, 0};
    const uint64_t request5[] = {0, 0, 0, 0, 5};
    const uint64_t requestA[] = {10, 20, 30, 40, 45};
    const uint64_t requestB[] = {0, 0, 0, 30, 40};
    const uint64_t requestC[] = {10, 18, 30, 30, 30};
    planner_multi_t *ctx = NULL;
    std::stringstream ss;

    errno = 0;
    to_stream (0, tmax, resource_totals, (const char **)resource_types, len, ss);
    ctx = planner_multi_new (0, INT64_MAX, resource_totals, resource_types, len);
    ok ((ctx && !errno), "new with (%s)", ss.str ().c_str ());

    /* The resource state will become the following after
       planner_multi_add_span calls:

        0:     9, 18, 27, 36, 45
        1000: 10, 18, 27, 36, 45
        2000: 10, 20, 27, 36, 45
        3000: 10, 20, 30, 36, 45
        4000: 10, 20, 30, 40, 45
        5000: 10, 20, 30, 40, 50
     */

    ss.str ("");
    to_stream (0, 1000, request1, (const char **)resource_types, len, ss);
    rc = planner_multi_avail_during (ctx, 0, 1000, request1, len);
    ok ((!rc && !errno), "multi-resource avail works (%s)", ss.str ().c_str ());

    span = planner_multi_add_span (ctx, 0, 1000, request1, len);
    ok ((span != -1), "span added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (0, 2000, request2, (const char **)resource_types, len, ss);
    rc = planner_multi_avail_during (ctx, 0, 2000, request2, len);
    ok ((!rc && !errno), "multi-resource avail works (%s)", ss.str ().c_str ());

    span = planner_multi_add_span (ctx, 0, 2000, request2, len);
    ok ((span != -1), "span added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (0, 3000, request3, (const char **)resource_types, len, ss);
    rc = planner_multi_avail_during (ctx, 0, 3000, request3, len);
    ok ((!rc && !errno), "multi-resource avail works (%s)", ss.str ().c_str ());

    span = planner_multi_add_span (ctx, 0, 3000, request3, len);
    ok ((span != -1), "span added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (0, 4000, request4, (const char **)resource_types, len, ss);
    rc = planner_multi_avail_during (ctx, 0, 4000, request4, len);
    ok ((!rc && !errno), "multi-resource avail works (%s)", ss.str ().c_str ());

    span = planner_multi_add_span (ctx, 0, 4000, request4, len);
    ok ((span != -1), "span added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (0, 5000, request5, (const char **)resource_types, len, ss);
    rc = planner_multi_avail_during (ctx, 0, 5000, request5, len);
    ok ((!rc && !errno), "multi-resource avail works (%s)", ss.str ().c_str ());

    span = planner_multi_add_span (ctx, 0, 5000, request5, len);
    ok ((span != -1), "span added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (0, 1000, requestA, (const char **)resource_types, len, ss);
    t = planner_multi_avail_time_first (ctx, 0, 1000, requestA, len);
    ok ((t == 4000), "avail_time_first for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == 5000), "avail_time_next for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == -1 && errno == ENOENT), "avail_time_next for (%s)",
         ss.str ().c_str ());

    ss.str ("");
    to_stream (0, 1000, requestB, (const char **)resource_types, len, ss);
    t = planner_multi_avail_time_first (ctx, 0, 1000, requestB, len);
    ok ((t == 0), "avail_time_first for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == 1000), "avail_time_first for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == 2000), "avail_time_first for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == 3000), "avail_time_first for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == 4000), "avail_time_next for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == 5000), "avail_time_next for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == -1 && errno == ENOENT), "avail_time_next for (%s)",
         ss.str ().c_str ());

    t = planner_multi_avail_time_first (ctx, 3500, 1000, requestB, len);
    ok ((t == 4000), "avail_time_first for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == 5000), "avail_time_next for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == -1 && errno == ENOENT), "avail_time_next for (%s)",
         ss.str ().c_str ());

    ss.str ("");
    to_stream (0, 1000, requestC, (const char **)resource_types, len, ss);
    t = planner_multi_avail_time_first (ctx, 0, 1000, requestC, len);
    ok ((t == 3000), "avail_time_first for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == 4000), "avail_time_next for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == 5000), "avail_time_next for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == -1 && errno == ENOENT), "avail_time_next for (%s)",
         ss.str ().c_str ());

    return 0;
}

static int test_multi_partially_larger ()
{
    size_t len = 5;
    int rc = -1;
    int64_t t = -1;
    int64_t tmax = INT64_MAX;
    int64_t span = -1;
    const uint64_t resource_totals[] = {10, 20, 30, 40, 50};
    const char *resource_types[] = {"A", "B", "C", "D", "E"};
    const uint64_t request1[] = {1, 0, 0, 0, 0};
    const uint64_t request2[] = {0, 2, 0, 0, 0};
    const uint64_t request3[] = {0, 0, 3, 0, 0};
    const uint64_t request4[] = {0, 0, 0, 4, 0};
    const uint64_t request5[] = {0, 0, 0, 0, 5};
    const uint64_t requestA[] = {10, 20, 30, 40, 50};
    const uint64_t requestB[] = {9, 20, 27, 36, 50};
    const uint64_t requestC[] = {9, 19, 30, 40, 50};
    planner_multi_t *ctx = NULL;
    std::stringstream ss;

    errno = 0;
    to_stream (0, tmax, resource_totals, (const char **)resource_types, len, ss);
    ctx = planner_multi_new (0, INT64_MAX, resource_totals, resource_types, len);
    ok ((ctx && !errno), "new with (%s)", ss.str ().c_str ());

    /* The resource state will become the following after
       planner_multi_add_span calls:

        0:     9, 20, 30, 40, 50
        1000: 10, 18, 30, 40, 50
        2000: 10, 20, 27, 40, 50
        3000: 10, 20, 30, 36, 50
        4000: 10, 20, 30, 40, 45
        5000: 10, 20, 30, 40, 50
     */

    ss.str ("");
    to_stream (0, 1000, request1, (const char **)resource_types, len, ss);
    rc = planner_multi_avail_during (ctx, 0, 1000, request1, len);
    ok ((!rc && !errno), "multi-resource avail works (%s)", ss.str ().c_str ());

    span = planner_multi_add_span (ctx, 0, 1000, request1, len);
    ok ((span != -1), "span added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (1000, 1000, request2, (const char **)resource_types, len, ss);
    rc = planner_multi_avail_during (ctx, 1000, 1000, request2, len);
    ok ((!rc && !errno), "multi-resource avail works (%s)", ss.str ().c_str ());

    span = planner_multi_add_span (ctx, 1000, 1000, request2, len);
    ok ((span != -1), "span added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (2000, 1000, request3, (const char **)resource_types, len, ss);
    rc = planner_multi_avail_during (ctx, 2000, 1000, request3, len);
    ok ((!rc && !errno), "multi-resource avail works (%s)", ss.str ().c_str ());

    span = planner_multi_add_span (ctx, 2000, 1000, request3, len);
    ok ((span != -1), "span added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (3000, 1000, request4, (const char **)resource_types, len, ss);
    rc = planner_multi_avail_during (ctx, 3000, 1000, request4, len);
    ok ((!rc && !errno), "multi-resource avail works (%s)", ss.str ().c_str ());

    span = planner_multi_add_span (ctx, 3000, 1000, request4, len);
    ok ((span != -1), "span added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (4000, 1000, request5, (const char **)resource_types, len, ss);
    rc = planner_multi_avail_during (ctx, 4000, 1000, request5, len);
    ok ((!rc && !errno), "multi-resource avail works (%s)", ss.str ().c_str ());

    span = planner_multi_add_span (ctx, 4000, 1000, request5, len);
    ok ((span != -1), "span added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 1000, requestA, (const char **)resource_types, len, ss);
    t = planner_multi_avail_time_first (ctx, 0, 1000, requestA, len);
    ok ((t == 5000), "avail_time_first for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == -1 && errno == ENOENT), "avail_time_next for (%s)",
         ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 1000, requestB, (const char **)resource_types, len, ss);
    t = planner_multi_avail_time_first (ctx, 0, 1000, requestB, len);
    ok ((t == 0), "avail_time_first for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == 2000), "avail_time_next for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == 3000), "avail_time_next for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == 5000), "avail_time_next for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == -1 && errno == ENOENT), "avail_time_next for (%s)",
         ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 1000, requestC, (const char **)resource_types, len, ss);
    t = planner_multi_avail_time_first (ctx, 0, 1000, requestC, len);
    ok ((t == 0), "avail_time_first for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == 5000), "avail_time_next for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == -1 && errno == ENOENT), "avail_time_next for (%s)",
         ss.str ().c_str ());

    return 0;
}

static int test_multi_many_spans ()
{
    size_t len = 3;
    int rc = -1;
    int64_t t = -1;
    int64_t tmax = INT64_MAX;
    int64_t span = -1;
    bool bo = false;
    const uint64_t resource_totals[] = {99, 99, 99};
    const char *resource_types[] = {"A", "B", "C"};
    const uint64_t requestA[] = {99, 99, 99};
    const uint64_t requestB[] = {98, 1, 98};
    const uint64_t requestC[] = {50, 49, 50};
    planner_multi_t *ctx = NULL;
    std::stringstream ss;

    errno = 0;
    to_stream (0, tmax, resource_totals, (const char **)resource_types, len, ss);
    ctx = planner_multi_new (0, INT64_MAX, resource_totals, resource_types, len);
    ok ((ctx && !errno), "new with (%s)", ss.str ().c_str ());

    for (int i = 0; i < 1000; i++) {
        uint64_t request[] = {0, 0, 0};
        request[0] = i % 100;
        request[1] = 99 - (i % 100);
        request[2] = i % 100;

        rc = planner_multi_avail_during (ctx, i*1000, 1000,
                                         (const uint64_t *)request, len);
        bo = (bo || rc);
        span = planner_multi_add_span (ctx, i*1000, 1000,
                                       (const uint64_t *)request, len);
        bo = (bo || span == -1);
    }
    ok ((!bo && !errno), "many multi_add_spans work");

    ss.str ("");
    to_stream (-1, 1000, requestA, (const char **)resource_types, len, ss);
    t = planner_multi_avail_time_first (ctx, 0, 1000, requestA, len);
    ok ((t == 1000000), "avail_time_first for (%s)", ss.str ().c_str ());
    t = planner_multi_avail_time_next (ctx);
    ok ((t == -1) && errno == ENOENT , "avail_time_next for (%s)",
    ss.str ().c_str ());

    ss.str ("");
    bo = false;
    to_stream (-1, 1000, requestB, (const char **)resource_types, len, ss);
    t = planner_multi_avail_time_first (ctx, 0, 1000, requestB, len);
    bo = (bo || (t != 1000));

    for (int i = 1; i < 10; i++) {
        t = planner_multi_avail_time_next (ctx);
        bo = (bo || (t != (1000 * (100 * i + 1))));
    }
    t = planner_multi_avail_time_next (ctx);
    bo = (bo || (t != 1000000));
    t = planner_multi_avail_time_next (ctx);
    bo = (bo || !(t == -1 && errno == ENOENT));
    ok (!bo, "avail_time_next for (%s)", ss.str ().c_str ());

    ss.str ("");
    bo = false;
    to_stream (-1, 1000, requestC, (const char **)resource_types, len, ss);
    t = planner_multi_avail_time_first (ctx, 0, 1000, requestC, len);
    bo = (bo || (t != 49000));

    for (int i = 1; i < 10; i++) {
        t = planner_multi_avail_time_next (ctx);
        bo = (bo || (t != (1000 * (100 * i + 49))));
    }

    t = planner_multi_avail_time_next (ctx);
    bo = (bo || (t != 1000000));
    t = planner_multi_avail_time_next (ctx);
    bo = (bo || !(t == -1 && errno == ENOENT));
    ok (!bo, "avail_time_next for (%s)", ss.str ().c_str ());
    return 0;
}

static int test_multi_add_remove ()
{
    size_t len = 5;
    size_t size = 0;
    int rc = -1;
    int64_t tmax = INT64_MAX;
    int64_t span1 = -1, span2 = -1, span3 = -1;
    const uint64_t resource_totals[] = {10, 20, 30, 40, 50};
    const char *resource_types[] = {"A", "B", "C", "D", "E"};
    const uint64_t request1[] = {1, 0, 0, 0, 0};
    const uint64_t request2[] = {0, 2, 0, 0, 0};
    const uint64_t request3[] = {0, 0, 3, 0, 0};
    planner_multi_t *ctx = NULL;
    std::stringstream ss;

    errno = 0;
    to_stream (0, tmax, resource_totals, (const char **)resource_types, len, ss);
    ctx = planner_multi_new (0, INT64_MAX, resource_totals, resource_types, len);
    ok ((ctx && !errno), "new with (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (0, 1000, request1, (const char **)resource_types, len, ss);
    rc = planner_multi_avail_during (ctx, 0, 1000, request1, len);
    ok ((!rc && !errno), "multi-resource avail works (%s)", ss.str ().c_str ());

    span1 = planner_multi_add_span (ctx, 0, 1000, request1, len);
    ok ((span1 != -1), "span added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (1000, 1000, request2, (const char **)resource_types, len, ss);
    rc = planner_multi_avail_during (ctx, 1000, 1000, request2, len);
    ok ((!rc && !errno), "multi-resource avail works (%s)", ss.str ().c_str ());

    span2 = planner_multi_add_span (ctx, 1000, 1000, request2, len);
    ok ((span2 != -1), "span added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (2000, 1000, request3, (const char **)resource_types, len, ss);
    rc = planner_multi_avail_during (ctx, 2000, 1000, request3, len);
    ok ((!rc && !errno), "multi-resource avail works (%s)", ss.str ().c_str ());

    span3 = planner_multi_add_span (ctx, 2000, 1000, request3, len);
    ok ((span3 != -1), "span added for (%s)", ss.str ().c_str ());

    rc = planner_multi_rem_span (ctx, span2);
    ok ((!rc && !errno), "multi_rem_span works");

    size = planner_multi_span_size (ctx);
    ok ((size == 2), "planner_multi_span_size works");

    return 0;

}


int main (int argc, char *argv[])
{
    plan (79);

    test_multi_basics ();

    test_multi_getters ();

    test_multi_strictly_larger ();

    test_multi_partially_larger ();

    test_multi_many_spans ();

    test_multi_add_remove ();

    done_testing ();

    return EXIT_SUCCESS;
}

/*
 * vi: ts=4 sw=4 expandtab
 */
