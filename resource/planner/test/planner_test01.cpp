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
#include "planner.h"
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

static void ordered_counts (planner_t *plan,
                     std::map<std::string, uint64_t> &unordered,
                     std::vector<uint64_t> &ordered)
{
    size_t len = planner_resources_len (plan);
    const char **resource_types = planner_resource_types (plan);
    for (unsigned int i = 0; i < len; ++i) {
        if (unordered.find (resource_types[i]) != unordered.end ())
            ordered.push_back (unordered[resource_types[i]]);
        else
            ordered.push_back (0);
    }
}

static int test_planner_getters ()
{
    unsigned int i = 0;
    size_t len = 5;
    bool bo = false;
    int64_t rc = -1;
    int64_t avail = -1;
    std::stringstream ss;
    planner_t *ctx = NULL;
    const char *type = NULL;
    const uint64_t resource_totals[] = {10, 20, 30, 40, 50};
    const char *resource_types[] = {"1", "2", "3", "4", "5"};

    errno = 0;
    to_stream (0, 9999, resource_totals, (const char **)resource_types, len, ss);
    ctx = planner_new (0, 9999, resource_totals, resource_types, len);
    ok ((ctx && !errno), "new with (%s)", ss.str ().c_str ());

    rc = planner_base_time (ctx);
    ok ((rc == 0), "base_time works for (%s)", ss.str ().c_str ());

    rc = planner_duration (ctx);
    ok ((rc == 9999), "duration works for (%s)", ss.str ().c_str ());

    rc = planner_resources_len (ctx);
    ok ((rc == 5), "resources_len works for (%s)", ss.str ().c_str ());

    for (i = 0; i < len; i++) {
        avail = planner_resource_total_at (ctx, i);
        bo = (bo || (avail != (10 * (i + 1))));

        avail = planner_resource_total_by_type (ctx, resource_types[i]);
        bo = (bo || (avail != (10 * (i + 1))));

        type = planner_resource_type_at (ctx, i);
        bo = (bo || (strcmp (type, resource_types[i]) != 0));
    }
    ok ((!bo && !errno), "planner getters work");

    planner_destroy (&ctx);
    return 0;
}

static int test_basic_add_remove ()
{
    int rc;
    int64_t t;
    size_t len = 1;
    std::stringstream ss;
    planner_t *ctx = NULL;
    const char *resource_types[] = {"B"};
    const uint64_t resource_totals[] = {1};
    const uint64_t *counts1 = resource_totals;
    int64_t span1 = -1, span2 = -1, span3 = -1, span4 = -1, span5 = -1;

    errno = 0;
    to_stream (0, 10, resource_totals, (const char **)resource_types, len, ss);
    ctx = planner_new (0, 10, resource_totals, resource_types, len);
    ok ((ctx && !errno), "new with (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 5, counts1, (const char **)resource_types, len, ss);
    t = planner_avail_time_first (ctx, 0, 5, counts1, len);
    ok (t == 0, "first scheduled point is @%d for (%s)", t, ss.str ().c_str ());

    span1 = planner_add_span (ctx, t, 5, counts1, len);
    ok (span1 != -1, "span1 added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 2, counts1, (const char **)resource_types, len, ss);
    t = planner_avail_time_first (ctx, 0, 2, counts1, len);
    ok (t == 5, "second point is @%d for (%s)", t, ss.str ().c_str ());

    span2 = planner_add_span (ctx, t, 2, counts1, len);
    ok (span2 != -1, "span2 added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 2, counts1, (const char **)resource_types, len, ss);
    t = planner_avail_time_first (ctx, 0, 2, counts1, len);
    ok (t == 7, "third point is @%d for (%s)", t, ss.str ().c_str ());

    span3 = planner_add_span (ctx, t, 2, counts1, len);
    ok (span3 != -1, "span3 added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 2, counts1, (const char **)resource_types, len, ss);
    t = planner_avail_time_first (ctx, 0, 2, counts1, len);
    ok (t == -1, "no scheduled point available for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 1, counts1, (const char **)resource_types, len, ss);
    t = planner_avail_time_first (ctx, 0, 1, counts1, len);
    ok (t == 9, "fourth point is @%d for (%s)", t, ss.str ().c_str ());

    span4 = planner_add_span (ctx, t, 1, counts1, len);
    ok (span4 != -1, "span4 added for (%s)", ss.str ().c_str ());

    ss.str ("");
    t = planner_span_start_time (ctx, span2);
    ok (t == 5, "span_start_time returned %ju", (intmax_t)t);

    rc = planner_rem_span (ctx, span2);
    ok (!rc, "span2 removed");

    rc = planner_rem_span (ctx, span3);
    ok (!rc, "span3 removed");

    to_stream (-1, 5, counts1, (const char **)resource_types, len, ss);
    t = planner_avail_time_first (ctx, 0, 5, counts1, len);
    ok (t == -1, "no scheduled point available for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 4, counts1, (const char **)resource_types, len, ss);
    t = planner_avail_time_first (ctx, 0, 4, counts1, len);
    ok (t == 5, "fifth point is @%d for (%s)", t, ss.str ().c_str ());

    span5 = planner_add_span (ctx, t, 4, counts1, len);
    ok (span5 != -1, "span5 added for (%s)", ss.str ().c_str ());
    ss.str ("");

    planner_destroy (&ctx);
    return 0;
}

static int test_availability_checkers ()
{
    int rc;
    bool bo = false;
    size_t len = 1;
    int64_t t = -1;
    int64_t avail = -1, tmax = INT64_MAX;
    int64_t span1 = -1, span2 = -1, span3 = -1;
    int64_t avail_resources[] = {0};
    const uint64_t resource_totals[] = {10};
    const uint64_t counts1[] = {1};
    const uint64_t counts4[] = {4};
    const uint64_t counts5[] = {5};
    const uint64_t counts9[] = {9};
    const uint64_t *counts10 = resource_totals;
    const char *resource_types[] = {"A"};
    planner_t *ctx = NULL;
    std::stringstream ss;

    errno = 0;
    to_stream (0, tmax, resource_totals, (const char **)resource_types, len, ss);
    ctx = planner_new (0, tmax, resource_totals, resource_types, len);
    ok ((ctx && !errno), "new with (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 5, counts10, (const char **)resource_types, len, ss);
    rc = planner_avail_during (ctx, 0, 1, counts10, len);
    ok ((!rc && !errno), "avail check works (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 1000, counts5, (const char **)resource_types, len, ss);
    rc = planner_avail_during (ctx, 1, 1000, counts5, len);
    ok ((!rc && !errno), "avail check works (%s)", ss.str ().c_str ());

    span1 = planner_add_span (ctx, 1, 1000, counts5, len);
    ok ((span1 != -1), "span1 added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 1000, counts10, (const char **)resource_types, len, ss);
    rc = planner_avail_during (ctx, 2000, 1001, counts10, len);
    span2 = planner_add_span (ctx, 2000, 1001, counts10, len);
    ok ((span2 != -1), "span2 added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 2990, counts1, (const char **)resource_types, len, ss);
    rc = planner_avail_during (ctx, 10, 2991, counts1, len);
    ok ((rc == -1) && !errno, "over-alloc fails for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 1990, counts1, (const char **)resource_types, len, ss);
    rc = planner_avail_during (ctx, 10, 1990, counts1, len);
    ok ((!rc && !errno), "overlapping works (%s)", ss.str ().c_str ());

    span3 = planner_add_span (ctx, 10, 1990, counts1, len);
    ok ((span3 != -1), "span3 added for (%s)", ss.str ().c_str ());

    ss.str ("");
    avail = planner_avail_resources_at (ctx, 1, 0);
    bo = (bo || avail != 5);
    avail = planner_avail_resources_at (ctx, 10, 0);
    bo = (bo || avail != 4);
    planner_avail_resources_array_at (ctx, 500, avail_resources, len);
    bo = (bo || avail_resources[0] != 4);
    avail = planner_avail_resources_at_by_type (ctx, 1000, "A");
    bo = (bo || avail != 4);
    avail = planner_avail_resources_at (ctx, 1500, 0);
    bo = (bo || avail != 9);
    avail = planner_avail_resources_at_by_type (ctx, 1998, "A");
    bo = (bo || avail != 9);
    avail = planner_avail_resources_at (ctx, 2000, 0);
    bo = (bo || avail != 0);
    avail = planner_avail_resources_at (ctx, 2500, 0);
    bo = (bo || avail != 0);
    avail = planner_avail_resources_at_by_type (ctx, 2999, "A");
    bo = (bo || avail != 0);
    avail = planner_avail_resources_at (ctx, 3000, 0);
    bo = (bo || avail != 0);
    avail = planner_avail_resources_at (ctx, 3001, 0);
    bo = (bo || avail != 10);
    ok (!bo && !errno, "avail_at_resources_* works");

    bo = false;
    rc = planner_avail_during (ctx, 2000, 1001, counts1, len);
    bo = (bo || rc != -1);
    avail = planner_avail_resources_during (ctx, 2000, 1001, 0);
    bo = (bo || avail != 0);
    rc = planner_avail_during (ctx, 0, 1001, counts4, len);
    bo = (bo || rc != 0);
    avail = planner_avail_resources_during (ctx, 0, 1001, 0);
    bo = (bo || avail != 4);
    rc = planner_avail_during (ctx, 10, 1990, counts4, len);
    bo = (bo || rc != 0);
    avail = planner_avail_resources_during (ctx, 10, 1990, 0);
    bo = (bo || avail != 4);
    ok (!bo && !errno, "resources_during works");

    bo = false;
    rc = planner_avail_during (ctx, 4, 3, counts5, len);
    bo = (bo || rc != 0);
    avail = planner_avail_resources_during (ctx, 4, 3, 0);
    bo = (bo || avail != 5);
    rc = planner_avail_during (ctx, 20, 980, counts4, len);
    bo = (bo || rc != 0);
    avail = planner_avail_resources_during (ctx, 20, 980, 0);
    bo = (bo || avail != 4);
    rc = planner_avail_during (ctx, 1001, 998, counts9, len);
    bo = (bo || rc != 0);
    avail = planner_avail_resources_during (ctx, 1001, 998, 0);
    bo = (bo || avail != 9);
    rc = planner_avail_during (ctx, 2500, 101, counts1, len);
    bo = (bo || rc != -1);
    avail = planner_avail_resources_during (ctx, 2500, 101, 0);
    bo = (bo || avail != 0);
    ok (!bo && !errno, "resources_during works for a subset (no edges)");

    bo = false;
    rc = planner_avail_during (ctx, 0, 1000, counts4, len);
    bo = (bo || rc != 0);
    avail = planner_avail_resources_during_by_type (ctx, 0, 1000, "A");
    bo = (bo || avail != 4);
    rc = planner_avail_during (ctx, 10, 990, counts4, len);
    bo = (bo || rc != 0);
    avail = planner_avail_resources_during_by_type (ctx, 10, 990, "A");
    bo = (bo || avail != 4);
    rc = planner_avail_during (ctx, 20, 981, counts4, len);
    bo = (bo || rc != 0);
    avail = planner_avail_resources_during_by_type (ctx, 20, 981, "A");
    bo = (bo || avail != 4);
    rc = planner_avail_during (ctx, 1001, 999, counts9, len);
    bo = (bo || rc != 0);
    planner_avail_resources_array_during (ctx, 1001, 999, avail_resources, len);
    bo = (bo || avail_resources[0] != 9);
    ok (!bo && !errno, "resources_during works for a subset (1 edge)");

    bo = false;
    rc = planner_avail_during (ctx, 100, 1401, counts4, len);
    bo = (bo || rc != 0);
    rc = planner_avail_during (ctx, 1500, 1001, counts1, len);
    bo = (bo || rc != -1);
    rc = planner_avail_during (ctx, 1000, 1001, counts1, len);
    bo = (bo || rc != -1);
    ok (!bo && !errno, "resources_during works for >1 overlapping spans");

    bo = false;
    rc = planner_avail_during (ctx, 0, 3001, counts1, len);
    bo = (bo || rc != -1);
    rc = planner_avail_during (ctx, 0, 2001, counts1, len);
    bo = (bo || rc != -1);
    rc = planner_avail_during (ctx, 3001, 2000, counts10, len);
    bo = (bo || rc != 0);
    ok (!bo && !errno, "resources_during works for all spans");

    bo = false;
    t = planner_avail_time_first (ctx, 0, 9, counts5, len);
    bo = (bo || t != 0);
    t = planner_avail_time_next (ctx);
    bo = (bo || t != 1);
    t = planner_avail_time_next (ctx);
    bo = (bo || t != 1001);
    t = planner_avail_time_next (ctx);
    bo = (bo || t != 3001);
    t = planner_avail_time_next (ctx);
    bo = (bo || t != -1);
    ok (!bo && !errno, "avail_time_* works");

    bo = false;
    t = planner_avail_time_first (ctx, 0, 10, counts9, len);
    bo = (bo || t != 1001);
    t = planner_avail_time_next (ctx);
    bo = (bo || t != 3001);
    t = planner_avail_time_next (ctx);
    bo = (bo || t != -1);
    ok (!bo && !errno, "avail_time_* test 2 works");

    planner_destroy (&ctx);
    return 0;
}

static int test_multi_resources ()
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
    planner_t *ctx = NULL;
    std::stringstream ss;

    errno = 0;
    to_stream (0, tmax, resource_totals, (const char **)resource_types, len, ss);
    ctx = planner_new (0, INT64_MAX, resource_totals, resource_types, len);
    ok ((ctx && !errno), "new with (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 5, counts10, (const char **)resource_types, len, ss);
    rc = planner_avail_during (ctx, 0, 1, counts10, len);
    ok ((!rc && !errno), "checking multi avail works (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 1000, counts5, (const char **)resource_types, len, ss);
    rc = planner_avail_during (ctx, 1, 1000, counts5, len);
    ok ((!rc && !errno), "multi-resource avail works (%s)", ss.str ().c_str ());

    span1 = planner_add_span (ctx, 1, 1000, counts5, len);
    ok ((span1 != -1), "span1 added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 1000, counts10, (const char **)resource_types, len, ss);
    rc = planner_avail_during (ctx, 2000, 1001, counts10, len);
    span2 = planner_add_span (ctx, 2000, 1001, counts10, len);
    ok ((span2 != -1), "span2 added for (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 2990, counts1, (const char **)resource_types, len, ss);
    rc = planner_avail_during (ctx, 10, 2991, counts1, len);
    ok ((rc == -1) && !errno, "over-alloc multi: (%s)", ss.str ().c_str ());

    ss.str ("");
    to_stream (-1, 1990, counts1, (const char **)resource_types, len, ss);
    rc = planner_avail_during (ctx, 10, 1990, counts1, len);
    ok ((!rc && !errno), "overlapped multi resources (%s)", ss.str ().c_str ());

    span3 = planner_add_span (ctx, 10, 1990, counts1, len);
    ok ((span3 != -1), "span3 added for (%s)", ss.str ().c_str ());

    ss.str ("");
    avail = planner_avail_resources_at (ctx, 1, 0);
    bo = (bo || avail != 5);
    avail = planner_avail_resources_at (ctx, 10, 1);
    bo = (bo || avail != 8);
    planner_avail_resources_array_at (ctx, 500, avail_resources, len);
    bo = (bo || avail_resources[2] != 12);
    avail = planner_avail_resources_at_by_type (ctx, 1000, "E");
    bo = (bo || avail != 40);
    avail = planner_avail_resources_at (ctx, 1500, 0);
    bo = (bo || avail != 9);
    avail = planner_avail_resources_at_by_type (ctx, 1998, "D");
    bo = (bo || avail != 18);
    avail = planner_avail_resources_at (ctx, 2000, 0);
    bo = (bo || avail != 0);
    avail = planner_avail_resources_at (ctx, 2500, 0);
    bo = (bo || avail != 0);
    avail = planner_avail_resources_at_by_type (ctx, 2999, "A");
    bo = (bo || avail != 0);
    avail = planner_avail_resources_at (ctx, 3000, 4);
    bo = (bo || avail != 0);
    avail = planner_avail_resources_at (ctx, 3001, 2);
    bo = (bo || avail != 30);
    ok (!bo && !errno, "avail_at_resources_* works");

    bo = false;
    t = planner_avail_time_first (ctx, 0, 9, counts_only_C, len);
    bo = (bo || t != 0);
    t = planner_avail_time_next (ctx);
    bo = (bo || t != 1);
    t = planner_avail_time_next (ctx);
    bo = (bo || t != 1001);
    t = planner_avail_time_next (ctx);
    bo = (bo || t != 3001);
    t = planner_avail_time_next (ctx);
    bo = (bo || t != -1);
    ok (!bo && !errno, "avail_time_* works");

    bo = false;
    t = planner_avail_time_first (ctx, 0, 10, counts_many_E, len);
    bo = (bo || t != 0);
    t = planner_avail_time_next (ctx);
    bo = (bo || t != 1001);
    t = planner_avail_time_next (ctx);
    bo = (bo || t != 3001);
    t = planner_avail_time_next (ctx);
    bo = (bo || t != -1);
    ok (!bo && !errno, "avail_time_* test2 works");

    planner_destroy (&ctx);
    return 0;
}

int test_add_and_iterator ()
{
    bool bo = false;
    size_t len = 1;
    int64_t t = -1;
    int64_t span1 = -1;
    const uint64_t counts3[] = {3};
    const uint64_t resource_totals[] = {10};
    const char *resource_types[] = {"C"};
    planner_t *ctx = NULL;
    std::stringstream ss;

    errno = 0;
    to_stream (0, 10, resource_totals, (const char **)resource_types, len, ss);
    ctx = planner_new (0, 10, resource_totals, resource_types, len);
    ok ((ctx && !errno), "new with (%s)", ss.str ().c_str ());

    ss.str ("");
    t = planner_avail_time_first (ctx, 0, 4, counts3, 1);
    bo = (bo || t != 0);
    t = planner_avail_time_next (ctx);
    bo = (bo || t != -1);
    span1 = planner_add_span (ctx, 0, 4, counts3, 1);
    bo = (bo || span1 == -1);
    t = planner_avail_time_first (ctx, 0, 4, counts3, 1);
    bo = (bo || t != 0);
    t = planner_avail_time_next (ctx);
    bo = (bo || t != 4);
    t = planner_avail_time_next (ctx);
    bo = (bo || t != -1);
    ok (!bo && !errno, "span_add resets the avail-time iterator");

    planner_destroy (&ctx);
    return 0;
}

int test_on_or_after ()
{
    int rc = 0;
    bool bo = false;
    int64_t t = -1;
    int64_t span1 = -1, span2 = -1;
    size_t len = 1;
    const uint64_t counts1[] = {1};
    const uint64_t counts2[] = {2};
    const uint64_t resource_totals[] = {3};
    const char *resource_types[] = {"A"};
    planner_t *ctx = NULL;
    std::stringstream ss;

    errno = 0;
    to_stream (0, INT64_MAX, resource_totals,
               (const char **)resource_types, len, ss);
    ctx = planner_new (0, INT64_MAX, resource_totals, resource_types, len);
    ok ((ctx && !errno), "new with (%s)", ss.str ().c_str ());

    ss.str ("");
    t = planner_avail_time_first (ctx, 100000, 100, counts2, len);
    bo = (bo || t != -1);
    rc = planner_avail_during (ctx, 100000, 100, counts2, len);
    bo = (bo || rc != 0);
    span1 = planner_add_span (ctx, 100000, 100, counts2, len);
    bo = (bo || span1 == -1);
    t = planner_avail_time_first (ctx, 100000, 200, counts2, len);
    bo = (bo || t != 100100);
    span2 = planner_add_span (ctx, 100100, 200, counts2, len);
    bo = (bo || span2 == -1);
    t = planner_avail_time_first (ctx, 100000, 200, counts1, len);
    bo = (bo || t != 100000);
    t = planner_avail_time_next (ctx);
    bo = (bo || t != 100100);
    t = planner_avail_time_next (ctx);
    bo = (bo || t != 100300);
    t = planner_avail_time_first (ctx, 0, 200, counts1, len);
    bo = (bo || t != 0);
    t = planner_avail_time_next (ctx);
    bo = (bo || t != 100000);
    ok (!bo && !errno, "on_or_after support works");

    planner_destroy (&ctx);
    return 0;
}

int test_remove_more ()
{
    int end = 0, i, rc;
    int64_t at, span;
    size_t len = 1;
    bool bo = false;
    const uint64_t resource_totals[] = {10};
    const char *resource_types[] = {"core"};
    uint64_t count = 5;
    int overlap_factor = resource_totals[0]/count;
    std::vector<int64_t> query_times;
    planner_t *ctx = NULL;
    std::stringstream ss;

    errno = 0;
    to_stream (0, INT64_MAX, resource_totals,
               (const char **)resource_types, len, ss);
    ctx = planner_new (0, INT64_MAX, resource_totals, resource_types, len);
    ok ((ctx && !errno), "new with (%s)", ss.str ().c_str ());
    ss.str ("");

    std::vector<int64_t> spans;
    for (i = 0; i < 10000; ++i) {
        at = i/overlap_factor * 1000;
        span = planner_add_span (ctx, at, 1000, (const uint64_t *)&count, len);
        spans.push_back (span);
        bo = (bo || span == -1);
    }

    for (i = 0; i < end; i += 4) {
        rc = planner_rem_span (ctx, spans[i]);
        bo = (bo || rc == -1);
    }

    ok (!bo && !errno, "removing more works");
    planner_destroy (&ctx);
    return 0;

}

int test_stress_fully_overlap ()
{
    int i = 0;
    bool bo = false;
    size_t len = 1;
    int64_t t = -1;
    int64_t span;
    const uint64_t resource_totals[] = {10000000};
    const uint64_t counts100[] = {100};
    const char *resource_types[] = {"hardware-thread"};
    planner_t *ctx = NULL;
    std::stringstream ss;

    errno = 0;
    to_stream (0, INT64_MAX, resource_totals,
               (const char **)resource_types, len, ss);
    ctx = planner_new (0, INT64_MAX, resource_totals, resource_types, len);
    ok ((ctx && !errno), "new with (%s)", ss.str ().c_str ());

    ss.str ("");
    for (i = 0; i < 100000; ++i) {
        t = planner_avail_time_first (ctx, 0, 4, counts100, 1);
        bo = (bo || t != 0);
        span = planner_add_span (ctx, t, 4, counts100, 1);
        bo = (bo || span == -1);
    }
    ok (!bo && !errno, "add_span 100000 times (fully overlapped spans)");

    for (i = 0; i < 100000; ++i) {
        t = planner_avail_time_first (ctx, 0, 4, counts100, 1);
        bo = (bo || t != 4);
        span = planner_add_span (ctx, t, 4, counts100, 1);
        bo = (bo || span == -1);
    }
    ok (!bo && !errno, "add_span 100000 more (fully overlapped spans)");

    planner_destroy (&ctx);
    return 0;
}

int test_stress_4spans_overlap ()
{
    int i = 0;
    int rc = 0;
    int64_t span;
    size_t len = 1;
    bool bo = false;
    const uint64_t resource_totals[] = {10000000};
    const char *resource_types[] = {"hardware-thread"};
    const uint64_t counts100[] = {100};
    planner_t *ctx = NULL;
    std::stringstream ss;

    errno = 0;
    to_stream (0, INT64_MAX, resource_totals,
               (const char **)resource_types, len, ss);
    ctx = planner_new (0, INT64_MAX, resource_totals, resource_types, len);
    ok ((ctx && !errno), "new with (%s)", ss.str ().c_str ());

    for (i = 0; i < 100000; ++i) {
        rc = planner_avail_during (ctx, i, 4, counts100, 1);
        bo = (bo || rc != 0);
        span = planner_add_span (ctx, i, 4, counts100, 1);
        bo = (bo || span == -1);
    }
    ok (!bo && !errno, "add_span 100000 times (4 spans overlap)");

    for (i = 100000; i < 200000; ++i) {
        rc = planner_avail_during (ctx, i, 4, counts100, 1);
        bo = (bo || rc != 0);
        span = planner_add_span (ctx, i, 4, counts100, 1);
        bo = (bo || span == -1);
    }
    ok (!bo && !errno, "add_span 100000 more (4 spans overlap)");

    planner_destroy (&ctx);
    return 0;
}

static int test_resource_service_flow ()
{
    int rc = 0;
    int L1_size = 9, L2_size = 3, depth = 50000;
    bool bo = false;
    int64_t at = 3600, t = -1, span = -1;
    uint64_t duration = 1000;
    std::map<int, uint64_t> totals;
    std::map<int, const char *> types;
    std::vector<uint64_t> global_totals;
    std::vector<const char *> global_types;
    std::vector <planner_t *> locals;
    planner_t *global = NULL;
    std::stringstream ss;

    types[0] = "A";
    types[1] = "B";
    types[2] = "C";
    totals[0] = 16;
    totals[1] = 2;
    totals[2] = 32;

    for (auto &kv : types)
        global_types.push_back (strdup (kv.second));
    for (auto &kv : totals)
        global_totals.push_back (L2_size * kv.second);
    for (int i = 0; i < L1_size; ++i)
        locals.push_back (planner_new (0, INT64_MAX, &(totals[i % L2_size]),
                                       &(types[i%L2_size]), 1));

    global = planner_new (0, INT64_MAX, &(global_totals[0]), &(global_types[0]),
                          global_types.size ());

    // Update both local/global planners for allocation
    for (int i = 0; i < L1_size; ++i) {
        planner_add_span (locals[i], at, duration, &(totals[i % L2_size]), 1);
        std::vector<uint64_t> ordered;
        std::map<std::string, uint64_t> lookup;
        lookup[types[i % L2_size]] = totals[i % L2_size];
        ordered_counts (global, lookup, ordered);
        planner_add_span (global, at, duration, &(ordered[0]), ordered.size ());
    }
    at += 1000;

    // Update both local/global planners for reservation with a large depth
    for (int i = 0; i < depth; i++) {
        int j = i % L2_size;
        int k = i % L1_size;
        std::vector<uint64_t> o;
        std::map<std::string, uint64_t> lookup;

        lookup[types[j]] = totals[j];
        ordered_counts (global, lookup, o);

        // Determine the earliest scheduleable point on or after the time, at
        t = planner_avail_time_first (global, at, duration, &(o[0]), o.size ());
        bo = (bo || t != (int64_t)(at + (i/L1_size)*duration));

        // Descend/Reserve lower-level resource at this schedule point
        rc = planner_avail_during (locals[k], t, duration, &(totals[j]), 1);
        bo = (bo || rc == -1);
        span = planner_add_span (locals[k], t, duration, &(totals[j]), 1);
        bo = (bo || span == -1);

        // Get back to global on postorder visit and update the aggregate info
        span = planner_add_span (global, t, duration, &(o[0]), o.size ());
        bo = (bo || span == -1);
    }
    ok (!bo && !errno, "reserve %d jobs for global/local planners", depth);

    planner_destroy (&global);
    for (auto &p : locals)
        planner_destroy (&p);

    return 0;
}

static int test_more_add_remove ()
{
    int rc;
    int64_t span1, span2, span3, span4, span5, span6;
    size_t len = 4;
    bool bo = false;
    const uint64_t resource_totals[] = {100000, 10000, 10000, 1000};
    const uint64_t resource1[] = {36, 0, 0, 0};
    const uint64_t resource2[] = {3600, 0, 0, 0};
    const uint64_t resource3[] = {1800, 0, 0, 0};
    const uint64_t resource4[] = {1152, 0, 0, 100};
    const uint64_t resource5[] = {2304, 0, 0, 64};
    const uint64_t resource6[] = {468, 0, 0, 13};
    const char *resource_types[] = {"core", "gpu", "socket", "node"};
    planner_t *ctx = NULL;
    std::stringstream ss;

    errno = 0;
    to_stream (0, INT64_MAX, resource_totals,
               (const char **)resource_types, len, ss);
    ctx = planner_new (0, INT64_MAX, resource_totals, resource_types, len);
    ok ((ctx && !errno), "new with (%s)", ss.str ().c_str ());
    ss.str ("");

    span1 = planner_add_span (ctx, 0, 600, resource1, len);
    bo = (bo || span1 == -1);
    span2 = planner_add_span (ctx, 0, 57600, resource2, len);
    bo = (bo || span2 == -1);
    span3 = planner_add_span (ctx, 57600, 57600, resource3, len);
    bo = (bo || span3 == -1);
    span4 = planner_add_span (ctx, 115200, 57600, resource4, len);
    bo = (bo || span4 == -1);
    span5 = planner_add_span (ctx, 172800, 57600, resource5, len);
    bo = (bo || span5 == -1);
    span6 = planner_add_span (ctx, 115200, 900, resource6, len);
    bo = (bo || span6 == -1);

    rc = planner_rem_span (ctx, span1);
    bo = (bo || rc == -1);
    rc = planner_rem_span (ctx, span2);
    bo = (bo || rc == -1);
    rc = planner_rem_span (ctx, span3);
    bo = (bo || rc == -1);
    rc = planner_rem_span (ctx, span4);
    bo = (bo || rc == -1);
    rc = planner_rem_span (ctx, span5);
    bo = (bo || rc == -1);
    rc = planner_rem_span (ctx, span6);
    bo = (bo || rc == -1);

    span1 = planner_add_span (ctx, 0, 600, resource1, len);
    bo = (bo || span1 == -1);
    span2 = planner_add_span (ctx, 0, 57600, resource2, len);
    bo = (bo || span2 == -1);
    span3 = planner_add_span (ctx, 57600, 57600, resource3, len);
    bo = (bo || span3 == -1);
    span4 = planner_add_span (ctx, 115200, 57600, resource4, len);
    bo = (bo || span4 == -1);
    span5 = planner_add_span (ctx, 172800, 57600, resource5, len);
    bo = (bo || span5 == -1);
    span6 = planner_add_span (ctx, 115200, 900, resource6, len);
    bo = (bo || span6 == -1);

    ok (!bo && !errno, "more add-remove-add test works");
    return 0;
}

int main (int argc, char *argv[])
{
    plan (63);

    test_planner_getters ();

    test_basic_add_remove ();

    test_availability_checkers ();

    test_multi_resources ();

    test_add_and_iterator ();

    test_on_or_after ();

    test_remove_more ();

    test_stress_fully_overlap ();

    test_stress_4spans_overlap ();

    test_resource_service_flow ();

    test_more_add_remove ();

    done_testing ();

    return EXIT_SUCCESS;
}

/*
 * vi: ts=4 sw=4 expandtab
 */
