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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argz.h>
#include <errno.h>
#include <libgen.h>
#include <czmq.h>
#include <flux/core.h>

#include "src/common/libutil/log.h"
#include "src/common/libutil/shortjansson.h"
#include "src/common/libutil/xzmalloc.h"
#include "resrc.h"
#include "resrc_tree.h"
#include "resrc_reqst.h"
#include "scheduler.h"

static int reservation_depth = 1;
static int curr_reservation_depth = 0;
static zlist_t *allocation_completion_times = NULL;
static zlist_t *all_completion_times = NULL;
static int64_t current_time = -1;

#if CZMQ_VERSION < CZMQ_MAKE_VERSION(3, 0, 1)
static bool compare_int64_ascending (void *item1, void *item2)
{
    int64_t time1 = *((int64_t *) item1);
    int64_t time2 = *((int64_t *) item2);

    return time1 > time2;
}
#else
static int compare_int64_ascending (void *item1, void *item2)
{
    int64_t time1 = *((int64_t *) item1);
    int64_t time2 = *((int64_t *) item2);

    return time1 - time2;
}
#endif

/* TEST - for validation of correctness only */
static char *allocation_filename = "allocations.csv";
static FILE *alloc_file = NULL;
/* TSET */

static bool select_children (flux_t *h, resrc_api_ctx_t *rsapi,
                             resrc_tree_list_t *children,
                             resrc_reqst_list_t *reqst_children,
                             resrc_tree_t *selected_parent);

resrc_tree_t *select_resources (flux_t *h, resrc_api_ctx_t *rsapi,
                                resrc_tree_t *found_tree,
                                resrc_reqst_t *resrc_reqst,
                                resrc_tree_t *selected_parent);

static resrc_tree_t *internal_select_resources (flux_t *h, resrc_api_ctx_t *rsapi,
                                                resrc_tree_t *candidate_tree,
                                                resrc_reqst_t *resrc_reqst,
                                                resrc_tree_t *selected_parent);

int sched_loop_setup (flux_t *h)
{
    curr_reservation_depth = 0;
    if (!allocation_completion_times)
        allocation_completion_times = zlist_new ();
    if (!all_completion_times) {
        all_completion_times = zlist_new ();
    } else {
        int64_t *current_completion_time;
        zlist_purge (all_completion_times);
        for (current_completion_time = zlist_first (allocation_completion_times);
             current_completion_time;
             current_completion_time = zlist_next (allocation_completion_times)) {
            if (current_time >= 0 && *current_completion_time < current_time) {
                zlist_remove (allocation_completion_times, current_completion_time);
            } else {
                zlist_append (all_completion_times, current_completion_time);
            }
        }
    }

    return 0;
}

/*
 * find_resources() identifies the exact set of resources that
 * satisfies the job resource request, either now or at a future
 * time. The found time will be stored locally in the plugin's memory
 * space.
 *
 * Inputs:  resrcs      - hash table of all resources
 *          resrc_reqst - the resources the job requests
 * Returns: nfound      - the number of resources found
 *          found_tree  - a resource tree containing resources that satisfy the
 *                        job's request or NULL if none are found
 */
int64_t find_resources (flux_t *h, resrc_api_ctx_t *rsapi,
                        resrc_t *resrc, resrc_reqst_t *resrc_reqst,
                        resrc_tree_t **found_tree)
{
    int64_t *completion_time = NULL;
    int64_t prev_completion_time = -1;
    bool only_now_scheduling = false;

    if (!resrc || !resrc_reqst) {
        flux_log (h, LOG_ERR, "%s: invalid arguments", __FUNCTION__);
        return 0;
    }

    // set the current time in our plugin local memory based on the
    // starttime of the resource request. this will be used later on
    // by select_resources.
    current_time = resrc_reqst_starttime (resrc_reqst);
    int64_t req_walltime = resrc_reqst_endtime (resrc_reqst) - current_time;

    if (reservation_depth == 0)
        /* All backfilling (no reservations).  Return success to
         * backfill all jobs remaining in the queue */
        only_now_scheduling = true;
    else if (reservation_depth < 0) {
        // always try to find resources now and in the future
        only_now_scheduling = false;
    } else {
        // only try to find resources now if we have already made all
        // the necessary reservations
        only_now_scheduling = (curr_reservation_depth >= reservation_depth);
    }

    // search for resources at the current time
    *found_tree = internal_select_resources (h, rsapi, resrc_phys_tree (resrc), resrc_reqst, NULL);
    if (resrc_reqst_all_found (resrc_reqst)) {
        // return non-zero in order to continue in sched.c through
        // to the selection/allocation/reservation
        flux_log (h, LOG_DEBUG, "%s: found all resources at current time (%"PRId64")", __FUNCTION__, current_time);
        return 1;
    }
    if (only_now_scheduling) {
        flux_log (h, LOG_DEBUG, "%s: couldn't find all resources at current time (%"PRId64"), not attempting to search into the future", __FUNCTION__, current_time);
        return 0;
    }
    flux_log (h, LOG_DEBUG, "%s: couldn't find all resources at current time (%"PRId64"), attempting to search into the future", __FUNCTION__, current_time);

    // search for resources at a future time
    zlist_sort (all_completion_times, compare_int64_ascending);

    for (completion_time = zlist_first (all_completion_times);
         completion_time;
         completion_time = zlist_next (all_completion_times)) {
        /* Purge past times from consideration */
        if (*completion_time < current_time) {
            zlist_remove (all_completion_times, completion_time);
            continue;
        }
        /* Don't test the same time multiple times */
        if (prev_completion_time == *completion_time)
            continue;

        resrc_reqst_set_starttime (resrc_reqst, *completion_time + 1);
        resrc_reqst_set_endtime (resrc_reqst, *completion_time + 1 + req_walltime);
        flux_log (h, LOG_DEBUG, "Attempting to find %"PRId64" nodes "
                  "at time %"PRId64"",
                  resrc_reqst_reqrd_qty (resrc_reqst),
                  *completion_time + 1);

        // found tree is the minimal subset of candidate resource that
        // satisfiy the resource request.  This WILL NOT be culled down any further
        resrc_reqst_clear_found (resrc_reqst);
        resrc_tree_unstage_resources (resrc_phys_tree (resrc));
        *found_tree = internal_select_resources (h, rsapi, resrc_phys_tree (resrc), resrc_reqst, NULL);
        if (resrc_reqst_all_found (resrc_reqst)) {
            // return non-zero in order to continue in sched.c through
            // to the selection/allocation/reservation
            return 1;
        }
        resrc_tree_destroy (rsapi, *found_tree, false, false);
        prev_completion_time = *completion_time;
    }

    return 0;
}

/*
 * cycles through all of the resource children and returns true when
 * the requested quantity of resources have been selected.
 */
static bool select_child (flux_t *h, resrc_api_ctx_t *rsapi,
                          resrc_tree_list_t *children,
                          resrc_reqst_t *child_reqst,
                          resrc_tree_t *selected_parent)
{
    resrc_tree_t *child_tree = NULL;
    bool selected = false;

    child_tree = resrc_tree_list_first (children);
    while (child_tree) {
        if (internal_select_resources (h, rsapi, child_tree,
                child_reqst, selected_parent) &&
            (resrc_reqst_nfound (child_reqst) >=
             resrc_reqst_reqrd_qty (child_reqst))) {
            selected = true;
            break;
        }
        child_tree = resrc_tree_list_next (children);
    }

    return selected;
}

/*
 * cycles through all of the resource requests and returns true if all
 * of the requested children were selected
 */
static bool select_children (flux_t *h, resrc_api_ctx_t *rsapi,
                             resrc_tree_list_t *children,
                             resrc_reqst_list_t *reqst_children,
                             resrc_tree_t *selected_parent)
{
    resrc_reqst_t *child_reqst = NULL;
    bool selected = false;

    child_reqst = resrc_reqst_list_first (reqst_children);
    while (child_reqst) {
        resrc_reqst_clear_found (child_reqst);
        selected = false;

        if (!select_child (h, rsapi, children, child_reqst, selected_parent))
            break;
        selected = true;
        child_reqst = resrc_reqst_list_next (reqst_children);
    }

    return selected;
}

/*
 * internal_select_resources() selects the minimal set of resources
 * that satisfy the job request (at the time given in resrc_reqst)
 *
 * Inputs:  candidate_tree   - tree of resource tree candidates
 *          resrc_reqst      - the resources the job requests
 *          selected_parent  - parent of the selected resource tree
 * Returns: a resource tree of however many resources were selected
 */
static resrc_tree_t *internal_select_resources (flux_t *h, resrc_api_ctx_t *rsapi,
                                                resrc_tree_t *candidate_tree,
                                                resrc_reqst_t *resrc_reqst,
                                                resrc_tree_t *selected_parent)
{
    resrc_t *resrc;
    resrc_tree_list_t *children = NULL;
    resrc_tree_t *child_tree;
    resrc_tree_t *selected_tree = NULL;

    if (!resrc_reqst) {
        flux_log (h, LOG_ERR, "%s: called with empty request", __FUNCTION__);
        return NULL;
    }

    resrc = resrc_tree_resrc (candidate_tree);
    if (resrc_match_resource (resrc, resrc_reqst, true)) {
        if (resrc_reqst_num_children (resrc_reqst)) {
            if (resrc_tree_num_children (candidate_tree)) {
                selected_tree = resrc_tree_new (selected_parent, resrc);
                if (select_children (h, rsapi, resrc_tree_children (candidate_tree),
                                     resrc_reqst_children (resrc_reqst),
                                     selected_tree)) {
                    resrc_stage_resrc (resrc,
                                       resrc_reqst_reqrd_size (resrc_reqst),
                                       resrc_reqst_graph_reqs (resrc_reqst));
                    resrc_reqst_add_found (resrc_reqst, 1);
                } else {
                    resrc_tree_destroy (rsapi, selected_tree, false, false);
                }
            }
        } else {
            selected_tree = resrc_tree_new (selected_parent, resrc);
            resrc_stage_resrc (resrc, resrc_reqst_reqrd_size (resrc_reqst),
                                       resrc_reqst_graph_reqs (resrc_reqst));
            resrc_reqst_add_found (resrc_reqst, 1);
        }
    } else if (resrc_tree_num_children (candidate_tree)) {
        /*
         * This clause visits the children of the current resource
         * searching for a match to the resource request.  The selected
         * tree must be extended to include this intermediate
         * resource.
         *
         * This also allows the resource request to be sparsely
         * defined.  E.g., it might only stipulate a node with 4 cores
         * and omit the intervening socket.
         */
        selected_tree = resrc_tree_new (selected_parent, resrc);
        children = resrc_tree_children (candidate_tree);
        child_tree = resrc_tree_list_first (children);
        while (child_tree) {
            if (internal_select_resources (h, rsapi, child_tree,
                    resrc_reqst, selected_tree) &&
                resrc_reqst_nfound (resrc_reqst) >=
                resrc_reqst_reqrd_qty (resrc_reqst))
                break;
            child_tree = resrc_tree_list_next (children);
        }
    }

    return selected_tree;
}

resrc_tree_t *select_resources (flux_t *h, resrc_api_ctx_t *rsapi,
                                resrc_tree_t *found_tree,
                                resrc_reqst_t *resrc_reqst,
                                resrc_tree_t *selected_parent)
{
    resrc_t *resrc = resrc_tree_resrc (found_tree);
    resrc_tree_t *selected_tree = internal_select_resources (h, rsapi, resrc_phys_tree (resrc), resrc_reqst, NULL);
    if (resrc_reqst_starttime (resrc_reqst) > current_time) {
        // trigger the reserve_resources in sched.c by making
        // resrc_reqst_all_found return false
        resrc_reqst_clear_found (resrc_reqst);
    }
    // trigger the allocate_resources in sched.c since
    // resrc_reqst_all_found will return true
    return selected_tree;
}

int allocate_resources (flux_t *h, resrc_api_ctx_t *rsapi,
                        resrc_tree_t *selected_tree, int64_t job_id,
                        int64_t starttime, int64_t endtime)
{
    int rc = -1;

    if (selected_tree) {
        fprintf (alloc_file, "%"PRId64",%"PRId64",",
                job_id, starttime);
        rc = resrc_tree_allocate_print (selected_tree, job_id,
                                        starttime, endtime, alloc_file);
        fprintf (alloc_file, "\n");
        if (!rc) {
            int64_t *completion_time = xzmalloc (sizeof(int64_t));
            *completion_time = endtime;
            rc = zlist_append (allocation_completion_times, completion_time);
            zlist_freefn (allocation_completion_times, completion_time, free, true);
            rc |= zlist_append (all_completion_times, completion_time);
            flux_log (h, LOG_DEBUG, "Allocated job %"PRId64" from %"PRId64" to "
                      "%"PRId64"", job_id, starttime, *completion_time);
        }
    }

    return rc;
}

/*
 * reserve_resources() reserves resources for the specified job id.
 *
 * unused variables: starttime, resrc
 */
int reserve_resources (flux_t *h, resrc_api_ctx_t *rsapi,
                       resrc_tree_t **selected_tree, int64_t job_id,
                       int64_t starttime, int64_t walltime, resrc_t *resrc,
                       resrc_reqst_t *resrc_reqst)
{
    int rc = -1;
    int64_t reservation_starttime = resrc_reqst_starttime (resrc_reqst);
    int64_t reservation_completion_time = reservation_starttime + 1 + walltime;

    if (!resrc || !resrc_reqst) {
        flux_log (h, LOG_ERR, "%s: invalid arguments", __FUNCTION__);
        return rc;
    }
    if (*selected_tree) {
        rc = resrc_tree_reserve (*selected_tree, job_id,
                                 reservation_starttime + 1,
                                 reservation_completion_time);
        if (rc) {
            resrc_tree_destroy (rsapi, *selected_tree, false, false);
            *selected_tree = NULL;
            flux_log (h, LOG_ERR, "Reservation for job %"PRId64" failed", job_id);
        } else {
            curr_reservation_depth++;
            int64_t *completion_time = xzmalloc (sizeof(int64_t));
            *completion_time = reservation_completion_time;
            rc |= zlist_append (all_completion_times, completion_time);
            flux_log (h, LOG_DEBUG, "Reserved %"PRId64" nodes for job "
                      "%"PRId64" from %"PRId64" to %"PRId64"",
                      resrc_reqst_reqrd_qty (resrc_reqst), job_id,
                      reservation_starttime + 1,
                      reservation_starttime + 1 + walltime);
        }
    }

    return rc;
}


// Reservation Depth Guide:
//     0 = All backfilling (no reservations)
//     1 = EASY Backfill
//    >1 = Hybrid Backfill
//    <0 = Conservative Backfill
int process_args (flux_t *h, char *argz, size_t argz_len, const sched_params_t *sp)
{
    int rc = 0;
    char *reserve_depth_str = NULL;
    char *entry = NULL;

    for (entry = argz;
         entry;
         entry = argz_next (argz, argz_len, entry)) {

        if (!strncmp ("reserve-depth=", entry, sizeof ("reserve-depth"))) {
            reserve_depth_str = strstr (entry, "=") + 1;
        } else if (!strncmp ("alloc-file=", entry, sizeof ("alloc-file"))) {
            allocation_filename = xasprintf ("%s", strstr (entry, "=") + 1);
            // XXX: This is never freed
        } else {
            rc = -1;
            errno = EINVAL;
            goto done;
        }
    }

    if (reserve_depth_str) {
        // If atoi fails, it defaults to 0, which is fine for us
        reservation_depth = atoi (reserve_depth_str);
    } else {
        reservation_depth = 0;
    }

    if (!sp) {
        flux_log (h, LOG_ERR, "scheduling parameters unavailable");
        rc = -1;
        errno = EINVAL;
    } else if (reservation_depth == -1) {
        /* Conservative backfill (-1) will still be limited by the queue-depth
         * but we just treat queue-depth as the limit for it
         */
        reservation_depth = sp->queue_depth;
    } else if (reservation_depth > sp->queue_depth) {
        flux_log (h, LOG_ERR,
                  "reserve-depth value (%d) - greater than queue-depth (%ld)",
                  reservation_depth, sp->queue_depth);
        rc = -1;
        errno = EINVAL;
    }

    alloc_file = fopen(allocation_filename, "w");
    if (alloc_file == NULL) {
        flux_log (h, LOG_ERR, "Unable to write to '%s'", allocation_filename);
        rc = -1;
        goto done;
    } else { 
        flux_log (h, LOG_DEBUG, "Opened log file '%s'", allocation_filename);
    }
    fprintf(alloc_file, "jobid,starttime,nodelist\n");


 done:
    return rc;
}


MOD_NAME ("sched.fast_backfill");


/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
