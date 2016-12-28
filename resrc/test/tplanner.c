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

#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include "planner.h"
#include "src/common/libtap/tap.h"
#include "src/common/libutil/xzmalloc.h"

#define EXTRA_VALIDATION 0

static int64_t gl_plid = 0;

static inline plan_t *pt_plan_new (planner_t *ctx, req_t *req, int64_t start)
{
    plan_t *plan = NULL;

    if (!req || start < 0)
        goto done;

    plan = xzmalloc (sizeof (*plan));
    plan->req = req;
    plan->id = gl_plid;
    plan->start = start;
    gl_plid++;

done:
    return plan;
}

static req_t *pt_vreq_new (uint64_t duration, size_t len, va_list ap)
{
    int i = 0;
    req_t *req = NULL;
    if (duration < 1 || len > MAX_RESRC_DIM)
        goto done;

    req = xzmalloc (sizeof (*req));
    req->resrc_vector = xzmalloc (len * sizeof (*(req->resrc_vector)));
    req->vector_dim = len;
    req->duration = duration;
    for (i=0; i < len; ++i)
        req->resrc_vector[i] = (uint64_t)va_arg(ap, int);

done:
    return req;
}

/* make sure to pass only integers for optional arguments */
static req_t *pt_req_new (uint64_t duration, size_t len, ...)
{
    req_t *req = NULL;

    va_list ap;
    va_start(ap, len);
    req = pt_vreq_new (duration, len, ap);
    va_end(ap);
    return req;
}

static void pt_req_free (req_t *req)
{
    if (req) {
        free (req->resrc_vector);
        free (req);
    }
}

int pt_make_n_valid_rsvs (planner_t *ctx, reservation_t ***ra_p,
               int n, uint64_t duration, size_t len, ...)
{
    int i = 0;
    int rc = -1;
    req_t *req = NULL;
    plan_t *plan = NULL;
    reservation_t *rsv = NULL;

    va_list ap;
    va_start(ap, len);
    req = pt_vreq_new (duration, len, ap);
    va_end(ap);

    if (!req)
        goto done;

    (*ra_p) = xzmalloc (n * sizeof (**ra_p));
    for (i = 0; i < n; ++i) {
        if (!(plan = pt_plan_new (ctx, req, planner_avail_time_first (ctx, req))))
            goto done;
        else if (!(rsv = planner_reservation_new (ctx, plan)))
            goto done;
        else if (planner_add_reservation (ctx, rsv, EXTRA_VALIDATION) < 0)
            goto done;

        free (plan);
        plan = NULL;
        (*ra_p)[i] = rsv;
    }
    pt_req_free (req);
    req = NULL;
    rc = 0;

done:
    if (plan)
        free (plan);
    if (req)
        pt_req_free (req);
    return rc;
}


int pt_make_n_decr_rsvs (planner_t *ctx, reservation_t ***ra_p,
               int n, uint64_t start_duration, size_t len, ...)
{
    int i = 0;
    int rc = -1;
    req_t *req = NULL;
    plan_t *plan = NULL;
    reservation_t *rsv = NULL;

    va_list ap;
    va_start(ap, len);
    req = pt_vreq_new (start_duration, len, ap);
    va_end(ap);

    if (!req)
        goto done;

    (*ra_p) = xzmalloc (n * sizeof (**ra_p));
    for (i = 0; i < n; ++i) {
        if (!(plan = pt_plan_new (ctx, req, planner_avail_time_first (ctx, req))))
            goto done;
        else if (!(rsv = planner_reservation_new (ctx, plan)))
            goto done;
        else if (planner_add_reservation (ctx, rsv, EXTRA_VALIDATION) < 0)
            goto done;

        free (plan);
        plan = NULL;
        (*ra_p)[i] = rsv;
        req->duration--;
    }
    pt_req_free (req);
    req = NULL;
    rc = 0;

done:
    if (plan)
        free (plan);
    if (req)
        pt_req_free (req);
    return rc;
}

static void test_1r0_10p_basic ()
{
    int i = 0;
    int rc = 0;
    int64_t starttime = 0;
    uint64_t total_resrcs = 1;
    req_t *req = NULL;
    req_t *req2 = NULL;
    plan_t *plan = NULL;
    reservation_t **ra = NULL;

    planner_t *ctx = planner_new (0, 10, &total_resrcs, 1);
    ok (ctx != NULL, "a planner for {<1>, 0-9}: 1-d 1 resrc for span of 10");

    rc = pt_make_n_valid_rsvs (ctx, &ra, 5, 2, 1, 1);
    ok (rc == 0, "add the max num of reservations, each requesting {<1>, 2}");

    rc = planner_rem_reservation (ctx, ra[1]);
    ok (rc == 0, "remove a reservation at 2 for {<1>, 2}");

    rc = planner_rem_reservation (ctx, ra[2]);
    ok (rc == 0, "remove a reservation at 4 for {<1>, 2}");

    req = pt_req_new (2, 1, 1);
    starttime = planner_avail_time_first (ctx, req);
    ok (starttime == 2, "find the first available time for {<1>, 2}");

    starttime = planner_avail_time_next (ctx);
    ok (starttime == -1, "should not find the next available time");

    rc = planner_avail_resources_at (ctx, 2, req);
    ok (rc == 0, "find availability at 2, for {<1>, 2}");

    rc = planner_avail_resources_at (ctx, 3, req);
    ok (rc == 0, "find availability at 3, for {<1>, 2}");

    rc = planner_avail_resources_at (ctx, 4, req);
    ok (rc == 0, "find availability at 4, for {<1>, 2}");

    rc = planner_avail_resources_at (ctx, 5, req);
    ok ((rc == -1) && !errno, "find no availability at 5, for {<1>, 2}");

    req2 = pt_req_new (2, 1, 1);
    starttime = planner_avail_time_first (ctx, req2);
    pt_req_free (req2);
    ok (starttime == 2, "find the first available time for {<1>, 3}");

    req2 = pt_req_new (4, 1, 1);
    starttime = planner_avail_time_first (ctx, req2);
    pt_req_free (req2);
    ok (starttime == 2, "find the first available time for {<1>, 4}");

    req2 = pt_req_new (5, 1, 1);
    starttime = planner_avail_time_first (ctx, req2);
    pt_req_free (req2);
    ok (starttime == -1, "no availability for {<1>, 5}");

    plan = pt_plan_new (ctx, req, 3);
    reservation_t *new_rsv = planner_reservation_new (ctx, plan);
    free (plan);
    rc = planner_add_reservation (ctx, new_rsv, EXTRA_VALIDATION);
    ok (rc == 0, "add reservation at 3, for {<1>, 2}");

    starttime = planner_avail_time_first (ctx, req);
    pt_req_free (req);
    ok (starttime == -1, "no availability for {<1>, 2}");

    planner_reservation_destroy (ctx, &new_rsv);
    for (i=0; i < 5; ++i)
        planner_reservation_destroy (ctx, &(ra[i]));

    free (ra);
    ra = NULL;
    planner_destroy (&ctx);
}

static void test_1kr0_10kp_larger ()
{
    int i = 0;
    int rc = 0;
    req_t *req = NULL;
    plan_t *plan = NULL;

    reservation_t **ra = NULL;
    int64_t starttime = -1;
    uint64_t total_resrcs = 1000;
    planner_t *ctx = planner_new (0, 10000, &total_resrcs, 1);
    ok (ctx != NULL, "a planner for {<1000>, 0-9999}: <1k> resrc for 10k");

    rc = pt_make_n_valid_rsvs (ctx, &ra, 9, 100, 1, 100);
    ok (rc == 0, "add 9 reservations, each requesting {<100>, 100}");

    req = pt_req_new (100, 1, 99);
    starttime = planner_avail_time_first (ctx, req);
    pt_req_free (req);
    ok (starttime == 0, "find the first available time for {<99>, 100}");

    req = pt_req_new (100, 1, 100);
    starttime = planner_avail_time_first (ctx, req);
    pt_req_free (req);
    ok (starttime == 0, "find the first available time for {<100>, 100}");

    req = pt_req_new (101, 1, 100);
    starttime = planner_avail_time_first (ctx, req);
    pt_req_free (req);
    ok (starttime == 0, "find the first available time for {<100>, 101}");

    req = pt_req_new (100, 1, 101);
    starttime = planner_avail_time_first (ctx, req);
    pt_req_free (req);
    ok (starttime == 100, "find the first available time for {<101>, 100}");

    req = pt_req_new (100, 1, 100);
    plan = pt_plan_new (ctx, req, 0);
    reservation_t *new_rsv = planner_reservation_new (ctx, plan);
    free (plan);
    pt_req_free (req);
    rc = planner_add_reservation (ctx, new_rsv, EXTRA_VALIDATION);
    ok (rc == 0, "add reservation at 0, for {<100>, 100}");

    req = pt_req_new (1000, 1, 1);
    starttime = planner_avail_time_first (ctx, req);
    ok (starttime == 100, "find the first available time for {<1>, 1000}");
    plan = pt_plan_new (ctx, req, starttime);
    reservation_t *new_rsv2 = planner_reservation_new (ctx, plan);
    free (plan);
    pt_req_free (req);
    rc = planner_add_reservation (ctx, new_rsv2, EXTRA_VALIDATION);
    ok (rc == 0, "add reservation at 100, for {<100>, 100}");

    planner_reservation_destroy (ctx, &new_rsv);
    planner_reservation_destroy (ctx, &new_rsv2);
    for (i=0; i < 9; ++i)
        planner_reservation_destroy (ctx, &(ra[i]));
    free (ra);
    ra = NULL;
    planner_destroy (&ctx);
}

void test_5r0_90p_noncontiguous ()
{
    int rc = 0;
    req_t *req = NULL;
    plan_t *plan = NULL;

    int64_t starttime = -1;
    uint64_t total_resrcs = 5;
    planner_t *ctx = planner_new (0, 90, &total_resrcs, 1);
    ok (ctx != NULL, "a planner for {<5>, 0-89}: <5> resrc for 90 span");

    req = pt_req_new (10, 1, 5);
    rc = planner_avail_resources_at (ctx, 0, req);
    ok (rc == 0, "find availability at 0, for {<5>, 10}");

    plan = pt_plan_new (ctx, req, 0);
    reservation_t *rsv1 = planner_reservation_new (ctx, plan);
    free (plan);
    rc = planner_add_reservation (ctx, rsv1, EXTRA_VALIDATION);
    ok (rc == 0, "add reservation at 0, for {<5>, 10}");

    rc = planner_avail_resources_at (ctx, 15, req);
    ok (rc == 0, "find availability at 0, for {<5>, 10}");
    plan = pt_plan_new (ctx, req, 15);
    reservation_t *rsv2 = planner_reservation_new (ctx, plan);
    free (plan);
    rc = planner_add_reservation (ctx, rsv2, EXTRA_VALIDATION);
    ok (rc == 0, "add reservation at 15, for {<5>, 10}");

    rc = planner_avail_resources_at (ctx, 35, req);
    ok (rc == 0, "find availability at 0, for {<5>, 10}");
    plan = pt_plan_new (ctx, req, 35);
    reservation_t *rsv3 = planner_reservation_new (ctx, plan);
    free (plan);
    rc = planner_add_reservation (ctx, rsv3, EXTRA_VALIDATION);
    ok (rc == 0, "add reservation at 35, for {<5>, 10}");

    rc = planner_avail_resources_at (ctx, 60, req);
    ok (rc == 0, "find availability at 0, for {<5>, 10}");
    plan = pt_plan_new (ctx, req, 60);
    reservation_t *rsv4 = planner_reservation_new (ctx, plan);
    free (plan);
    pt_req_free (req);
    rc = planner_add_reservation (ctx, rsv4, EXTRA_VALIDATION);
    ok (rc == 0, "add reservation at 60, for {<5>, 10}");

    req = pt_req_new (5, 1, 5);
    starttime = planner_avail_time_first (ctx, req);
    pt_req_free (req);
    ok (starttime == 10, "find the first available time for {<5>, 5}");
    starttime = planner_avail_time_next (ctx);
    ok (starttime == 25, "find the next available time for {<5>, 5}");
    starttime = planner_avail_time_next (ctx);
    ok (starttime == 45, "find the next available time for {<5>, 5}");
    starttime = planner_avail_time_next (ctx);
    ok (starttime == 70, "find the next available time for {<5>, 5}");
    starttime = planner_avail_time_next (ctx);
    ok (starttime == -1, "find no available time for {<5>, 5}");

    req = pt_req_new (10, 1, 5);
    starttime = planner_avail_time_first (ctx, req);
    pt_req_free (req);
    ok (starttime == 25, "find the first available time for {<5>, 10}");
    starttime = planner_avail_time_next (ctx);
    ok (starttime == 45, "find the next available time for {<5>, 10}");
    starttime = planner_avail_time_next (ctx);
    ok (starttime == 70, "find the next available time for {<5>, 10}");
    starttime = planner_avail_time_next (ctx);
    ok (starttime == -1, "find no available time for {<5>, 10}");

    req = pt_req_new (15, 1, 5);
    starttime = planner_avail_time_first (ctx, req);
    pt_req_free (req);
    ok (starttime == 45, "find the next available time for {<5>, 15}");
    starttime = planner_avail_time_next (ctx);
    ok (starttime == 70, "find the next available time for {<5>, 15}");
    starttime = planner_avail_time_next (ctx);
    ok (starttime == -1, "find no available time for {<5>, 15}");

    req = pt_req_new (20, 1, 5);
    starttime = planner_avail_time_first (ctx, req);
    ok (starttime == 70, "find the next available time for {<5>, 20}");
    starttime = planner_avail_time_next (ctx);
    ok (starttime == -1, "find no available time for {<5>, 20}");
    plan = pt_plan_new (ctx, req, 70);
    reservation_t *rsv5 = planner_reservation_new (ctx, plan);
    free (plan);
    rc = planner_add_reservation (ctx, rsv5, EXTRA_VALIDATION);
    ok (rc == 0, "add reservation at 70, for {<5>, 20}");

    starttime = planner_avail_time_first (ctx, req);
    ok (starttime == -1, "find no available time for {<5>, 20}");
    rc = planner_rem_reservation (ctx, rsv4);
    ok (rc == 0, "remove reservation at 60");
    starttime = planner_avail_time_first (ctx, req);
    pt_req_free (req);
    ok (starttime == 45, "find the first available time for {<5>, 20}");

    planner_reservation_destroy (ctx, &rsv1);
    planner_reservation_destroy (ctx, &rsv2);
    planner_reservation_destroy (ctx, &rsv3);
    planner_reservation_destroy (ctx, &rsv4);
    planner_reservation_destroy (ctx, &rsv5);

    planner_destroy (&ctx);
}

void test_1r0_12p_midstart ()
{
    int rc = 0;
    req_t *req = NULL;
    plan_t *plan = NULL;
    int64_t starttime = -1;
    uint64_t total_resrcs = 1;

    planner_t *ctx = planner_new (0, 12, &total_resrcs, 1);
    ok (ctx != NULL, "a planner for {<1>, 0-9}: <1> resrc for 10 span");

    req = pt_req_new (4, 1, 1);
    rc = planner_avail_resources_at (ctx, 4, req);
    ok (rc == 0, "find availability at 4, for {<1>, 4}");

    plan = pt_plan_new (ctx, req, 4);
    reservation_t *rsv1 = planner_reservation_new (ctx, plan);
    free (plan);
    rc = planner_add_reservation (ctx, rsv1, EXTRA_VALIDATION);
    ok (rc == 0, "add reservation at 4, for {<1>, 4}");

    starttime = planner_avail_time_first (ctx, req);
    ok (starttime == 0, "find the first available time at 0 for {<1>, 4}");

    starttime = planner_avail_time_next (ctx);
    ok (starttime == 8, "find the next available time at 8 for {<1>, 4}");

    rc = planner_rem_reservation (ctx, rsv1);
    ok (rc == 0, "remove reservation at 4: {<1>, 4}");

    starttime = planner_avail_time_first (ctx, req);
    ok (starttime == 0, "find the first available time at 0 for {<1>, 4}");

    starttime = planner_avail_time_next (ctx);
    ok (starttime == -1, "no other scheduled point exists for {<1>, 4}");

    planner_reservation_destroy (ctx, &rsv1);
    plan = pt_plan_new (ctx, req, 3);
    rsv1 = planner_reservation_new (ctx, plan);
    free (plan);
    rc = planner_add_reservation (ctx, rsv1, EXTRA_VALIDATION);
    ok (rc == 0, "add reservation at 3, for {<1>, 4}");

    starttime = planner_avail_time_first (ctx, req);
    ok (starttime == 7, "find the first available time at 7 for {<1>, 4}");
    pt_req_free (req);

    planner_reservation_destroy (ctx, &rsv1);
    planner_destroy (&ctx);
}

void test_100r0_5000000_long ()
{
    int i = 0;
    int rc = 0;
    req_t *req = NULL;
    plan_t *plan = NULL;
    reservation_t **ra = NULL;
    int64_t starttime = -1;
    uint64_t total_resrcs = 100;

    planner_t *ctx = planner_new (0, 6000000, &total_resrcs, 1);
    ok (ctx != NULL, "a planner for {<100>, 0-4999999}: <100> for 5000000");

    rc = pt_make_n_valid_rsvs (ctx, &ra, 10000, 10000, 1, 5);
    ok (rc == 0, "add 10000 reservations, each requesting {<100>, 10000}");

    req = pt_req_new (10000, 1, 100);
    starttime = planner_avail_time_first (ctx, req);
    ok (starttime == 5000000, "find the first available time for {<100>, 10000}");

    plan = pt_plan_new (ctx, req, starttime);
    reservation_t *new_rsv = planner_reservation_new (ctx, plan);
    free (plan);
    pt_req_free (req);
    rc = planner_add_reservation (ctx, new_rsv, EXTRA_VALIDATION);
    ok (rc == 0, "add a reservation at 5000000 requesting {<100>, 10000}");

    for (i=0; i < 10000; ++i) {
        planner_rem_reservation (ctx, ra[i]);
        planner_reservation_destroy (ctx, &(ra[i]));
    }

    free (ra);
    ra = NULL;

    req = pt_req_new (10000, 1, 55);
    starttime = planner_avail_time_first (ctx, req);
    pt_req_free (req);
    ok (starttime == 0, "find the first available time for {<55>, 10000}");

    starttime = planner_avail_time_next (ctx);
    ok (starttime == 5010000, "find the first available time for {<55>, 10000}");

    starttime = planner_avail_time_next (ctx);
    ok (starttime == -1, "find the first available time for {<55>, 10000}");

    planner_reservation_destroy (ctx, &new_rsv);

    planner_destroy (&ctx);
}

void test_5r0_2200_short ()
{
    int i = 0;
    int rc = 0;
    req_t *req = NULL;
    plan_t *plan = NULL;
    reservation_t **ra = NULL;
    int64_t starttime = -1;
    uint64_t total_resrcs = 5;

    planner_t *ctx = planner_new (0, 2200, &total_resrcs, 1);
    ok (ctx != NULL, "a planner for {<5>, 0-2199}: <5> for 2200");

    rc = pt_make_n_valid_rsvs (ctx, &ra, 1000, 1, 1, 3);
    ok (rc == 0, "add 1000 reservations, each requesting {<3>, 1}");

    req = pt_req_new (10, 1, 2);
    starttime = planner_avail_time_first (ctx, req);
    ok (starttime == 0, "find the first available time for {<2>, 10}");

    plan = pt_plan_new (ctx, req, starttime);
    reservation_t *rsv1 = planner_reservation_new (ctx, plan);
    free (plan);
    pt_req_free (req);
    rc = planner_add_reservation (ctx, rsv1, EXTRA_VALIDATION);
    ok (rc == 0, "add a reservation at 0 requesting {<2>, 10}");

    req = pt_req_new (10, 1, 4);
    starttime = planner_avail_time_first (ctx, req);
    ok (starttime == 1000, "find the first available time for {<4>, 10}");

    plan = pt_plan_new (ctx, req, starttime);
    reservation_t *rsv2 = planner_reservation_new (ctx, plan);
    free (plan);
    pt_req_free (req);
    rc = planner_add_reservation (ctx, rsv2, EXTRA_VALIDATION);
    ok (rc == 0, "add a reservation at 1000 requesting {<4>, 10}");

    req = pt_req_new (990, 1, 2);
    starttime = planner_avail_time_first (ctx, req);
    pt_req_free (req);
    ok (starttime == 10, "find the first available time for {<2>, 910}");

    req = pt_req_new (991, 1, 2);
    starttime = planner_avail_time_first (ctx, req);
    pt_req_free (req);
    ok (starttime == 1010, "find the first available time for {<2>, 911}");

    //rc = planner_print_gnuplot (ctx, "plan.out", 0);
    //ok (rc == 0, "print gnuplot works");

    for (i=0; i < 1000; ++i) {
        planner_rem_reservation (ctx, ra[i]);
        planner_reservation_destroy (ctx, &(ra[i]));
    }

    free (ra);
    ra = NULL;

    planner_reservation_destroy (ctx, &rsv1);
    planner_reservation_destroy (ctx, &rsv2);
    planner_destroy (&ctx);
}

static void test_5xr0_4_10p_basic ()
{
    int i = 0;
    int rc = 0;
    int64_t starttime = 0;
    uint64_t total_resrcs_a[5] = {5, 50, 500, 5000, 50000};
    req_t *req = NULL;
    req_t *req2 = NULL;
    plan_t *plan = NULL;
    reservation_t **ra = NULL;

    planner_t *ctx = planner_new (0, 10, total_resrcs_a, 5);
    ok (ctx != NULL, "a planner for {<5,50,500,5000,50000>, 0-9}: 5-d for 10");

    rc = pt_make_n_valid_rsvs (ctx, &ra, 5, 2, 5, 5, 50, 500, 5000, 50000);
    ok (rc == 0, "add reservations, each requesting {<5,50,500,5000,50000>, 2}");

    rc = planner_rem_reservation (ctx, ra[1]);
    ok (rc == 0, "remove a reservation at 2 for {<5,50,500,5000,50000>, 2}");

    rc = planner_rem_reservation (ctx, ra[2]);
    ok (rc == 0, "remove a reservation at 4 for {<5,50,500,5000,50000>, 2}");

    req = pt_req_new (2, 5, 5, 50, 500, 5000, 50000);
    starttime = planner_avail_time_first (ctx, req);
    ok (starttime == 2, "find first availability for {<5,50,500,5000,50000>, 2}");

    starttime = planner_avail_time_next (ctx);
    ok (starttime == -1, "should not find the next available time");

    rc = planner_avail_resources_at (ctx, 2, req);
    ok (rc == 0, "find availability at 2, for {<5,50,500,5000,50000>, 2}");

    rc = planner_avail_resources_at (ctx, 3, req);
    ok (rc == 0, "find availability at 3, for {<5,50,500,5000,50000>, 2}");

    rc = planner_avail_resources_at (ctx, 4, req);
    ok (rc == 0, "find availability at 4, for {<5,50,500,5000,50000>, 2}");

    rc = planner_avail_resources_at (ctx, 5, req);
    ok ((rc == -1) && !errno, "find no availability at 5");

    req2 = pt_req_new (3, 5, 5, 50, 500, 5000, 50000);
    starttime = planner_avail_time_first (ctx, req2);
    pt_req_free (req2);
    ok (starttime == 2, "find first availability for {<5,50,500,5000,50000>, 3}");

    req2 = pt_req_new (4, 5, 5, 50, 500, 5000, 50000);
    starttime = planner_avail_time_first (ctx, req2);
    pt_req_free (req2);
    ok (starttime == 2, "find first availability for {<5,50,500,5000,50000>, 4}");

    req2 = pt_req_new (5, 5, 5, 50, 500, 5000, 50000);
    starttime = planner_avail_time_first (ctx, req2);
    pt_req_free (req2);
    ok (starttime == -1, "no availability for {<5,50,500,5000,50000>, 5}");

    plan = pt_plan_new (ctx, req, 3);
    reservation_t *new_rsv = planner_reservation_new (ctx, plan);
    free (plan);
    rc = planner_add_reservation (ctx, new_rsv, EXTRA_VALIDATION);
    ok (rc == 0, "add reservation at 3, for {<5,50,500,5000,50000>, 2}");

    starttime = planner_avail_time_first (ctx, req);
    pt_req_free (req);
    ok (starttime == -1, "no availability for {<5,50,500,5000,50000>, 2}");

    planner_reservation_destroy (ctx, &new_rsv);
    for (i=0; i < 5; ++i)
        planner_reservation_destroy (ctx, &(ra[i]));

    free (ra);
    ra = NULL;
    planner_destroy (&ctx);
}

void test_2xr0_4_10p_2D_unmet ()
{
    int rc = 0;
    int64_t starttime = 0;
    uint64_t total_resrcs_a[5] = {2, 20, 200, 2000, 20000};
    req_t *req = NULL;
    req_t *req2 = NULL;
    plan_t *plan = NULL;

    planner_t *ctx = planner_new (0, 10, total_resrcs_a, 5);
    ok (ctx != NULL, "a planner for {<2,20,200,2000,20000>, 0-9}: 5-d for 10");

    req = pt_req_new (2, 5, 1, 10, 100, 1000, 10000);
    starttime = planner_avail_time_first (ctx, req);
    ok (starttime == 0, "find first availability for {<1,10,100,1000,10000>, 2}");

    plan = pt_plan_new (ctx, req, starttime);
    reservation_t *rsv1 = planner_reservation_new (ctx, plan);
    free (plan);
    rc = planner_add_reservation (ctx, rsv1, EXTRA_VALIDATION);
    ok (rc == 0, "add reservation at 0, for {<1,10,100,1000,10000>, 2}");

    starttime = planner_avail_time_first (ctx, req);
    ok (starttime == 0, "find first availability for {<1,10,100,1000,10000>, 2}");

    plan = pt_plan_new (ctx, req, starttime);
    reservation_t *rsv2 = planner_reservation_new (ctx, plan);
    free (plan);
    rc = planner_add_reservation (ctx, rsv2, EXTRA_VALIDATION);
    ok (rc == 0, "add reservation at 0, for {<1,10,100,1000,10000>, 2}");

    req2 = pt_req_new (2, 5, 0, 20, 100, 1000, 10000);
    starttime = planner_avail_time_first (ctx, req2);
    ok (starttime == 2, "find first availability for {<0,20,100,1000,10000>, 2}");

    plan = pt_plan_new (ctx, req2, starttime);
    reservation_t *rsv3 = planner_reservation_new (ctx, plan);
    free (plan);
    pt_req_free (req2);
    rc = planner_add_reservation (ctx, rsv3, EXTRA_VALIDATION);
    ok (rc == 0, "add reservation at 2, for {<1,20,100,1000,10000>, 2}");

    starttime = planner_avail_time_first (ctx, req);
    ok (starttime == 4, "find first availability for {<1,10,100,1000,10000>, 2}");

    plan = pt_plan_new (ctx, req, starttime);
    reservation_t *rsv4 = planner_reservation_new (ctx, plan);
    free (plan);
    pt_req_free (req);
    rc = planner_add_reservation (ctx, rsv4, EXTRA_VALIDATION);
    ok (rc == 0, "add reservation at 4, for {<1,10,100,1000,10000>, 2}");

    req = pt_req_new (2, 5, 1, 0, 100, 1000, 10000);
    starttime = planner_avail_time_first (ctx, req);
    pt_req_free (req);
    ok (starttime == 2, "find first availability for {<1,0,100,1000,10000>, 2}");

    planner_reservation_destroy (ctx, &rsv1);
    planner_reservation_destroy (ctx, &rsv2);
    planner_reservation_destroy (ctx, &rsv3);
    planner_reservation_destroy (ctx, &rsv4);

    planner_destroy (&ctx);
}

void test_many_complete_times ()
{
    int i = 0;
    int rc = 0;
    req_t *req = NULL;
    reservation_t **ra = NULL;
    int64_t starttime = -1;
    uint64_t total_resrcs = 2000;

    planner_t *ctx = planner_new (0, 2500, &total_resrcs, 1);
    ok (ctx != NULL, "a planner for {<2000>, 0-2499}: <2000> for 2500");

    rc = pt_make_n_decr_rsvs (ctx, &ra, 2000, 2000, 1, 1);
    ok (rc == 0, "add 2000 reservations, each requesting {<1>, 2000--}");

    for (i=1; i < 1999; ++i) {
        req = pt_req_new (10, 1, i);
        starttime = planner_avail_time_first (ctx, req);
        pt_req_free (req);
    }
    req = pt_req_new (10, 1, i);
    starttime = planner_avail_time_first (ctx, req);
    pt_req_free (req);
    ok (starttime == i, "find the first available time for {<1000++>, 10}");

    for (i=0; i < 2000; ++i)
        planner_reservation_destroy (ctx, &(ra[i]));

    free (ra);
    ra = NULL;

    planner_destroy (&ctx);
}

void test_misc ()
{
    int i = 0;
    int rc = 0;
    int64_t starttime = 0;
    uint64_t total_resrcs = 1;
    reservation_t **ra = NULL;
    reservation_t *rsv = NULL;

    planner_t *ctx = planner_new (0, 20, &total_resrcs, 1);
    ok (ctx != NULL, "a planner for {<1>, 0-9}: 1-d 1 resrc for span of 10");

    rc = pt_make_n_valid_rsvs (ctx, &ra, 5, 2, 1, 1);
    ok (rc == 0, "add the max num of reservations, each requesting {<1>, 2}");

    req_t *req = pt_req_new (2, 1, 1);
    plan_t *plan = pt_plan_new (ctx, req, 12);
    reservation_t *rsv1 = planner_reservation_new (ctx, plan);
    rc = planner_add_reservation (ctx, rsv1, 1);
    plan->start = 10;
    reservation_t *rptr = planner_reservation_by_id (ctx, plan->id);
    ok (rsv1 == rptr, "planner_reservation_by_id works");
    char key[32];
    sprintf (key, "%jd", (intmax_t)plan->id);
    rptr = planner_reservation_by_id_str (ctx, key);
    ok (rsv1 == rptr, "planner_reservation_by_id works");
    rptr = planner_reservation_new (ctx, plan);
    ok ((rptr == NULL) && (errno == EINVAL), "existing id correctly rejected");
    free (plan);
    pt_req_free (req);
    char *str = planner_reservation_to_string (ctx, rsv1);
    ok (str != NULL, "planner_reservation_to_string works");
    free (str);

    for (rsv = planner_reservation_first (ctx); rsv;
         rsv = planner_reservation_next (ctx, rsv)) {
        int64_t st = planner_reservation_starttime (ctx, rsv);
        if (st != -1 && st < starttime)
            break;
        starttime = st;
        i++;
    }
    ok ((i == 6), "planner_revervation iterator works");
    for (i=0; i < 5; ++i)
        planner_reservation_destroy (ctx, &(ra[i]));
    free (ra);
    ra = NULL;
    planner_reservation_destroy (ctx, &rsv1);
    planner_destroy (&ctx);
}

int main (int argc, char *argv[])
{

    plan (NO_PLAN);

    test_1r0_10p_basic ();

    test_1kr0_10kp_larger ();

    test_5r0_90p_noncontiguous ();

    test_1r0_12p_midstart ();

    test_100r0_5000000_long ();

    test_5r0_2200_short ();

    test_5xr0_4_10p_basic ();

    test_2xr0_4_10p_2D_unmet ();

    test_many_complete_times ();

    test_misc ();

    done_testing ();

    return 0;
}


/*
 * vi: ts=4 sw=4 expandtab
 */
