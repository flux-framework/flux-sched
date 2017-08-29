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

/* Overview:
 * This plugin implements a topologically-aware scheduler which supports the
 * same backfilling options as sched.backfill based on the reservation_depth but
 * with additional restrictions on job placement to avoid inter-job interference
 *
 * This plugin assumes a fat-tree topology with at least the following levels:
 *         L1         <- highest non-root level, e.g. "pod"
 *       /    \
 *      /      \
 *     L2       L2    <- next-highest level, e.g. "switch"
 *    /  \     /  \
 *   N .. N   N .. N  <- "node" level, the granularity at which we schedule
 *
 * We assume the level above "pod" is the root (e.g. "cluster") and there is no
 * point considering this "L0" level. Currently the scheduler only supports
 * trees which have at most three levels from the pod down to the node. Children
 * of "node" are allowed but this plugin only searches down to "node."
 *
 * We distinguish between three tiers of jobs. Let
 *   N = number of nodes requested
 *   levels[i] be the number of children of a vertex at level Li
 *   T1: (N < levels[2]) A job fitting within a single L2 tree ("switch")
 *   T2: (levels[1] < N <= levels[1]*levels[2]) A job spanning multiple L2
 *        trees but still within one L1 tree ("pod")
 *   T3: (N > levels[1]*levels[2]) A job spanning multiple L1 trees ("pods")
 * 
 * Scheduler Overview and Explanation:
 * The scheduler in sched.c does the following (the schedule_jobs function)
 *
 * sched_loop_setup                   // Clear all previous reservations
 * for (job in Queue)                 // Queue is not handled by plugin
 *   if (find_resources > 0)          (1)
 *     if (select_resources != NULL)  (2)
 *       if (resrc_request_all_found) // select_resources deals with this
 *         allocate_resources         // Add allocation counters to topo_tree
 *       else
 *         reserve_resources          // Add reservation counters to topo_tree
 *
 *  (1) Synchronize topo_tree with resrc_tree and return all the resources
 *      available now. Note that if find_resources returns 0 there is no point
 *      trying to reserve since nothing can happen until a job completes anyway.
 *  (2) Here we select nodes either now or future. Returns a resource_tree that
 *      contains exactly the number of nodes allocate or reserve_resources needs
 * 
 * Overview of Algorithm:
 * Let L = least, A = available, P = pod, S = switch, N = # of nodes in a job
 * switch (job type)
 * case t1:
 *      from (LAP:MAP such that N_avail >= N)
 *          from (LAS:MAS such that N_avail >= N)
 *              for (nd in Nodes)
 *                  if (available) assign and update and increment nfound
 *                  if (nfound == N) we're done
 * case t2:
 *      from (LAP:MAP such that N_avail >= N)
 *          AS = {s | T(s) < T2}
 *          from (MAS:LAS in AS)
 *              for (nd in Nodes)
 *                  if (available) assign and update and increment nfound
 *                  if (nfound == N) we're done
 * case t3:
 *      AP = {p | T(p) < T3}
 *      from (p = MAP:LAP in AP)
 *          AS = {s | T(s) < T2 in p}
 *          from (MAS:LAS in AS)
 *              for (nd in Nodes)
 *                  if (available) assign and update and increment nfound
 *                  if (nfound == N) we're done
 *
 * If we can't find any, we need to reserve. Thus, make a copy of this tree,
 * T_f = copy(T)
 * t_1 = current time
 * loop over each completed time
 * update t_1
 * remove that job from T_f
 * Try to search and place the job again
 * If you can place the job, break. Then we have from T_f + job's walltime
 *
 * Details:
 * We ensure a job will never be "upgraded". For example, a T1 job will always
 * be scheduled on a single switch and T2 on a single pod.
 * Q: Do we allow a T2/3 job which may only need 2 switches/pods to take up 3?
 * A: As of now, no.
 * 
 * Best-fit:
 * Another improvement over the existing backfill algorithm is including a best-
 * fit algorithm; while find_resources may return 100 nodes for a 5 node job,
 * sched.topo will find the 5 nodes which leave the least amount of surrounding
 * space.
 *
 * Best-fit details:
 * The best-fit will, like SLURM's topology/tree plugin, find
 * the lowest level which can accommodate the job and then attempt to place it.
 * One important difference is sched.topo will not split a T1 job across
 * multiple L2 trees, nor will it place a T2 job multiple L1 trees.
 *
 * Some Complications:
 * Suppose there is a cluster with 8 pods with 72 nodes per
 * pod, and a T3 job which requires 200 nodes. Which of these 8 pods do we
 * select? We may only pick pods without other T3 jobs, but beyond that we
 * preferentially pick the emptiest pods first up until we have <72 nodes to
 * select. This minimizes the total number of pods occupied at the T3 level. For
 * the remainder, we use best-fit; which partial pod can we partially fill such
 * that the number of unallocated nodes within it is minimized.
 * Idea: maybe this can recurse? Minimize unallocated pods -> switches -> nodes
 *
 * Assumptions:
 * A sibling at each level is symmetric; that is, if one vertex has K children,
 * then it is expected that every other vertex at that level has K children.
 */

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
#include <unistd.h> // getcwd
#include <limits.h> // PATH_MAX

#include "src/common/libutil/log.h"
#include "src/common/libutil/shortjansson.h"
#include "src/common/libutil/xzmalloc.h"
#include "resrc.h"
#include "resrc_tree.h"
#include "resrc_reqst.h"
#include "scheduler.h"
#include "resrc_api.h" /* resrc_api_ctx_t */
#include "resrc_api_internal.h" /* The fields of resrc_api_ctx_t */
#include "rsreader.h" /* rsreader_resrc_bulkload */

static int compare_int64_ascending (void *item1, void *item2)
{
    int64_t time1 = *((int64_t *) item1);
    int64_t time2 = *((int64_t *) item2);

    return time1 - time2;
}

static int reservation_depth = 1;
static int curr_reservation_depth = 0;

/* TOPOTODO: This needs to be reworked. Alongside this
 * (allocation_completion_times and all_ will eventually be deleted), we shall
 * put just the job_allocation_list which also contains the completion times.
 * This should also be able to be sorted as usual. When we're working our way
 * through the select_resources we may delete the ones with completion_time <
 * current time just like the allocatio_c_times.
 */
static zlist_t *allocation_completion_times = NULL;
static zlist_t *all_completion_times = NULL;
static int64_t current_time = -1;
typedef enum {RESERVE, ALLOCATE} mark_enum_t;
/* Selected nodes may be an allocation or reservation; this is created in
 * select_resources then saved off in allocate_resources. We use the built-in
 * r_tier for reservations to more easily purge it at each schedule_jobs loop */
typedef struct {
    int64_t completion_time;
    int64_t job_id;
    zlist_t *selected_nodes;
} allocation_t;
/* Compare allocations by their completion times using job_id as a tiebreaker */
static int compare_allocation_ascending (void *item1, void *item2)
{
    allocation_t *alloc1 = (allocation_t *) item1;
    allocation_t *alloc2 = (allocation_t *) item2;
    int timecmp = alloc1->completion_time - alloc2->completion_time;
    return (timecmp == 0) ? (alloc1->job_id - alloc2->job_id) : timecmp;
}
static zlist_t *job_allocation_list;
static zlist_t *current_selected_nodes;
/* Topology-specific data structures and types */

typedef struct { /* Count the number of jobs in your children at each tier */
    int64_t T1;  /* No propagation of T1 jobs (they don't interfere) */
    int64_t T2;  /* We propagate T2 values up one level */
    int64_t T3;  /* We propagate T3 values up two levels */
} tier_count_t;
/* From a topological standpoint we only care about these types of resources. */
typedef enum {CLUSTER, POD, SWITCH, NODE, UNKNOWN} resrc_enum_t;
typedef struct topo_tree_ {
    resrc_tree_t *ct;      /* The corresponding resource tree */
    resrc_enum_t type;
    /* a_ keeps track of allocations, r_ keeps track of reservations */
    tier_count_t a_tier;     /* Tier of running job T1, T2, T3 (See Overview) */
    tier_count_t r_tier;
    struct topo_tree_ *parent;
    int64_t job_id;
    zlist_t *children;
} topo_tree_t;

static topo_tree_t *topo_tree;
static resrc_api_ctx_t *topo_rsapi;
static zhash_t *node2topo_hash; // key: node name (char *), item: topo_tree_t *
static int tree_depth;
/* levels[0] is the highest level (e.g. pods), levels[tree_depth-1] are nodes */
static int64_t *levels;

static bool select_children (flux_t *h, resrc_api_ctx_t *rsapi,
                             resrc_tree_list_t *children,
                             resrc_reqst_list_t *reqst_children,
                             resrc_tree_t *selected_parent);

resrc_tree_t *select_resources (flux_t *h, resrc_api_ctx_t *rsapi,
                                resrc_tree_t *found_tree,
                                resrc_reqst_t *resrc_reqst,
                                resrc_tree_t *selected_parent);

/* Select the resources either now (allocate) or in the future (reserve)
 * This is the main logic for the scheduler.
 */
static resrc_tree_t *select_resources_in_time (flux_t *h,
                                               resrc_api_ctx_t *rsapi,
                                               resrc_tree_t *candidate_tree,
                                               resrc_reqst_t *resrc_reqst,
                                               resrc_tree_t *selected_parent);

static int allocate_topo_structures (flux_t *h);

/* Initialize the number of jobs of each tier for a given resource. Assumes
 * the scheduler plugin is loaded when no jobs are running.
 * Note: This should be updated if someone decided the sched.topo plugin may
 *       be hot-loaded (that is, the scheduler is in the middle of running jobs)
 */
static inline tier_count_t initialize_tier_counts ()
{
    return (tier_count_t) { .T1 = 0, .T2 = 0, .T3 = 0 };
}

/* Given a number of nodes, calculate which tier the job is */
static int calculate_tier (int64_t n_nodes)
{
    int job_tier;
    if (n_nodes <= levels[tree_depth-1]) {
        job_tier = 1;
    } else if (n_nodes <= levels[tree_depth-1] * levels[tree_depth-2]) {
        job_tier = 2;
    } else {
        job_tier = 3;
    }
    return job_tier;
}

static inline resrc_enum_t resrc_enum_from_type (char *r_t)
{
    if (strcmp (r_t, "cluster") == 0) {
        return CLUSTER;
    } else if (strcmp (r_t, "pod") == 0) {
        return POD;
    } else if (strcmp (r_t, "switch") == 0) {
        return SWITCH;
    } else if (strcmp (r_t, "node") == 0) {
        return NODE;
    } else {
        return UNKNOWN;
    }
}

/* Creates a topo_tree which mirrors the rsapi tree.
 * Also populates a hash for fast lookup from resrc name -> topo,
 * This assumes you have already initialized the hash table
 */
static topo_tree_t *topo_tree_create (
        flux_t *h, zhash_t *node_hash, resrc_tree_t *tree, topo_tree_t *parent)
{
    int rc;
    topo_tree_t *new_tree;
    char *r_t = resrc_type (resrc_tree_resrc (tree));
    char *r_n;
    resrc_enum_t r_e = resrc_enum_from_type (r_t);

    int lvl;
    switch (r_e) {
    case CLUSTER: lvl = 0;  break;
    case POD:     lvl = 1;  break;
    case SWITCH:  lvl = 2;  break;
    case NODE:    lvl = 3;  break;
    case UNKNOWN: lvl = -1; return NULL;
    }
    // printf("Resource type is '%s'\n", r_t); // TEST

    new_tree = xzmalloc (sizeof(topo_tree_t));
    new_tree->ct = tree;
    new_tree->type = r_e;
    new_tree->a_tier = initialize_tier_counts ();
    new_tree->r_tier = initialize_tier_counts ();
    new_tree->job_id = -1;
    new_tree->parent = parent;
    new_tree->children = NULL;
    
    if (strcmp (r_t, "node") == 0) {
        r_n = resrc_name (resrc_tree_resrc (tree));
        rc = zhash_insert (node_hash, r_n, new_tree);
        /* We don't need a freefn because topo_tree_destroy frees the nodes */
        (void) zhash_freefn (node_hash, r_n, NULL); 
        if (rc) {
            flux_log (h, LOG_ERR, "Error adding '%s' to hash\n",
                    resrc_name (resrc_tree_resrc (tree)));
            return NULL;
        }
        // printf ("added '%s' to hash\n", resrc_name (resrc_tree_resrc (tree))); // TEST
    }

    /* Loop over all children and only add the non-UNKNOWN ones */
    topo_tree_t *topo_child;
    resrc_tree_list_t *children = resrc_tree_children (tree);
    resrc_tree_t *child = resrc_tree_list_first (children);
    while (child != NULL) {
        topo_child = topo_tree_create (h, node_hash, child, new_tree);
        if (topo_child != NULL) {
            if (new_tree->children == NULL) {
                new_tree->children = zlist_new ();
            }
            rc = zlist_append (new_tree->children, topo_child);
            (void) zlist_freefn (new_tree->children, topo_child, free, true);
            // printf("Added at level %d\n", lvl); // TEST
            if (rc) {
                flux_log (h, LOG_ERR, "Unable to create topology tree");
                return NULL;
            }
        }
        child = resrc_tree_list_next (children);
    }
    return new_tree;
}

static inline char *str_from_resrc_enum (resrc_enum_t r_e)
{
    switch (r_e) {
    case CLUSTER:
        return "cluster";
    case POD:
        return "| pod";
    case SWITCH:
        return "| | switch";
    case NODE:
        return "| | | node";
    case UNKNOWN:
        return "unknown";
    }
    return "unreachable but this satisfies the compiler";
}

void topo_tree_print (topo_tree_t *tt)
{
    if (tt == NULL) {
        return;
    }
    char *tt_type = str_from_resrc_enum (tt->type);
    printf ("%s -> %s, Alloc T1: %"PRId64", T2: %"PRId64", T3: %"PRId64"; "
            "Reserv T1: %"PRId64", T2: %"PRId64", T3: %"PRId64"\n",
            tt_type, resrc_name (resrc_tree_resrc (tt->ct)),
            tt->a_tier.T1, tt->a_tier.T2, tt->a_tier.T3,
            tt->r_tier.T1, tt->r_tier.T2, tt->r_tier.T3);
    zlist_t *children = tt->children;
    if (children == NULL) {
        return;
    }
    topo_tree_t *child = zlist_first (children);
    while (child != NULL) {
        topo_tree_print (child);
        child = zlist_next (children);
    }
}

/* Creates a copy of the topo tree and a corresponding hash table for fast
 * lookup of nodes. Expects new_node_hash to be already initialized.
 */
topo_tree_t *topo_tree_copy (topo_tree_t *tt, zhash_t *new_node_hash)
{
    if (tt == NULL) {
        return NULL;
    }
    topo_tree_t *new_tree;
    char *r_n; // resource name
    int rc;

    new_tree = xzmalloc (sizeof(topo_tree_t));
    new_tree->ct = tt->ct;
    new_tree->type = tt->type;
    new_tree->a_tier = tt->a_tier;
    new_tree->r_tier = tt->r_tier;
    new_tree->job_id = tt->job_id;
    new_tree->parent = tt->parent;
    
    if (new_tree->type == NODE) {
        r_n = resrc_name (resrc_tree_resrc (new_tree->ct));
        rc = zhash_insert (new_node_hash, r_n, new_tree);
        /* We don't need a freefn because topo_tree_destroy frees the nodes */
        (void) zhash_freefn (new_node_hash, r_n, NULL); 
        if (rc) {
            return NULL;
        }
    }

    /* Copy the children */
    topo_tree_t *new_child;
    if (tt->children != NULL) {
        new_tree->children = zlist_new ();
    } else {
        new_tree->children = NULL;
        return new_tree;
    }
    topo_tree_t *child = zlist_first (tt->children);
    while (child != NULL) {
        new_child = topo_tree_copy (child, new_node_hash); 
        zlist_append (new_tree->children, new_child);
        (void) zlist_freefn (new_tree->children, new_child, free, true);
        child = zlist_next (tt->children);
    }
    return new_tree;
}

/* Frees the topo tree structs only, does not affect the corresponding
 * resrc_tree or any other pointers inside it.
 */
void topo_tree_destroy (topo_tree_t *tt)
{
    if (tt == NULL) {
        return;
    }
    if (tt->children != NULL) {
        topo_tree_t *child = zlist_pop (tt->children);
        while (child != NULL) {
            topo_tree_destroy (child);
            child = zlist_pop (tt->children);
        }
        zlist_destroy (&(tt->children));
    }
    free (tt);
}

/* Removes all existing reservations (r_tier) from the topology tree */
static void purge_reservations (topo_tree_t *tt)
{
    if (tt == NULL) {
        return;
    }
    tt->r_tier = initialize_tier_counts ();
    zlist_t *children = tt->children;
    if (children == NULL)
        return;
    topo_tree_t *child = zlist_first (children);
    while (child != NULL) {
        purge_reservations (child);
        child = zlist_next (children);
    }
}

/* Update the topology tree with the current state of the resource tree.
 * This will only decrement tier counters in the topology tree, not increment
 * them. Incrementing is done by mark_topo_tree.
 * found_tree: The tree of available resources from resrc_tree_search
 * Returns 0 if okay, -1 if something bad happened.
 */
/* XXX: Keep in mind that resrc_tree_search will only match resources
 * with the same properties and tags, but there may be resources which
 * are different enough to not match but still can affect the job from
 * an interference perspective.
 * At least in simulation, resrc_tree_allocate only calls
 * resrc_tree_allocate_in_time, which never sets the resrc state
 */
/* TODO: Free the node vector for each job when you see a job has completed
 */
static int synchronize (resrc_tree_t *found_tree)
{
    char *r_t = resrc_type (resrc_tree_resrc (found_tree));
    topo_tree_t *tt;
    char *node_name;
    int rc = 0;
    if (strcmp (r_t, "node") == 0) { /* Base case */
        node_name = resrc_name (resrc_tree_resrc (found_tree));
        tt = zhash_lookup (node2topo_hash, node_name);
        /* Mark the resource as available */
        if (tt->a_tier.T1 > 0) {
            tt->a_tier.T1--;
            tt->parent->a_tier.T1--;
            tt->parent->parent->a_tier.T1--;
            // printf ("A T1 job completed on node '%s'\n", node_name); // TEST
        } else if (tt->a_tier.T2 > 0) {
            tt->a_tier.T2--;
            tt->parent->a_tier.T2--;
            tt->parent->parent->a_tier.T2--;
            // printf ("A T2 job completed on node '%s'\n", node_name); // TEST
        } else if (tt->a_tier.T3 > 0) {
            tt->a_tier.T3--;
            tt->parent->a_tier.T3--;
            tt->parent->parent->a_tier.T3--;
            // printf ("A T3 job completed on node '%s'\n", node_name); // TEST
        }
    } else { /* We're not at the "node" level, Recurse */
        resrc_tree_list_t *children = resrc_tree_children (found_tree);
        resrc_tree_t *child = resrc_tree_list_first (children);
        while (child != NULL) {
            rc = synchronize (child);
            child = resrc_tree_list_next (children);
        }
    }
    return rc;
}

/* Called at the beginning of each schedule_jobs. All reservations are released 
 * before this. This assumes the first time the plugin is loaded there are no
 * resources being used. Removes all the completion times of allocations which
 * have finished since the last scheduling loop.
 */
int sched_loop_setup (flux_t *h) {
    curr_reservation_depth = 0;
    if (!job_allocation_list) {
        job_allocation_list = zlist_new ();
    } else {
        allocation_t *alloc;
        for (alloc = zlist_first (job_allocation_list);
             alloc;
             alloc = zlist_next (job_allocation_list)) {
            if (current_time >= 0 && alloc->completion_time < current_time) {
                printf ("Removing job %"PRId64" from list at %"PRId64"\n",
                        alloc->job_id, current_time); // TEST
                zlist_remove (job_allocation_list, alloc);
            }
        }
    }

    /* UNNECESSARY */
    if (!allocation_completion_times) {
        allocation_completion_times = zlist_new ();
    }
    if (!all_completion_times) {
        all_completion_times = zlist_new ();
    } else {
        int64_t *curr_ctime;
        zlist_purge (all_completion_times);
        for (curr_ctime = zlist_first (allocation_completion_times);
             curr_ctime;
             curr_ctime = zlist_next (allocation_completion_times)) {
            if (current_time >= 0 && *curr_ctime < current_time) {
                zlist_remove (allocation_completion_times, curr_ctime);
            } else {
                zlist_append (all_completion_times, curr_ctime);
            }
        }
    }
    /* End UNNECESSARY */

    if (topo_tree != NULL) {
        purge_reservations (topo_tree);
    } // The tree is created in find_resources because it needs resrc_api_ctx_t 
    return 0;
}

/* TODO: This isn't called anywhere */
/* Given a "sparse" resource request, ensure it requests the higher-level
 * resources given its Tier (See Overview). Make sure to destroy it once
 * you're done.
 */
int get_job_tier (flux_t *h, resrc_reqst_t *rr)
{
    int64_t n_nodes = resrc_reqst_reqrd_qty (rr);
    int lvl = tree_depth - 1;
    int64_t node_capacity = levels[lvl];
    int tier = 1;
    while (n_nodes > node_capacity) {
        if (lvl < 1) {
            flux_log (h, LOG_ERR, "Requested %"PRId64" nodes but system has"
                    " only %"PRId64, n_nodes, node_capacity);
            return -1;
        }
        node_capacity *= levels[--lvl];
        tier++;
    }
    printf("%"PRId64" node job is tier %d\n", n_nodes, tier); // TEST
    return tier;
}

/*
 * find_resources() counts the number of resources currently available for the
 * job, makes a tree enumerating these resources, and synchronizes the internal
 * data structures with the resource tree. If there are are no resources or
 * there can be no reservations (for whatever reason) this will return 0.
 * As it stands these topological data structures assume homogeneity. For
 * example, a job requesting a "large" node when none are available will still
 * find 0 and no following "small" nodes can be backfilled. For an explanation
 * of this issue see https://github.com/flux-framework/flux-sched/issues/197
 * Inputs:  rsapi       - Contains the root of the physical resource tree
 *          resrc_reqst - the resources the job requests
 * Returns: nfound      - the number of resources found
 *          found_tree  - a resource tree containing resources that satisfy the
 *                        job's request or NULL if none are found
 */
int64_t find_resources (flux_t *h, resrc_api_ctx_t *rsapi,
                        resrc_t *resrc, resrc_reqst_t *resrc_reqst,
                        resrc_tree_t **found_tree)
{
    int64_t nfound = 0;
    if (topo_tree == NULL) {
        if (resrc_tree_root (rsapi)) {
            node2topo_hash = zhash_new ();
            topo_tree = topo_tree_create (
                    h, node2topo_hash, resrc_tree_root (rsapi), NULL);
        } else {
            flux_log (h, LOG_ERR, "rsapi->tree_root is NULL");
            return -1;
        }
    }
    if (!resrc || !resrc_reqst) {
        flux_log (h, LOG_ERR, "%s: invalid arguments", __FUNCTION__);
        return 0;
    }

    /* TODO: May want to update from resrc_tree_search to our own, more
     * efficient function */
    /* Find all available resources so we can update the topo_tree */
    nfound = resrc_tree_search (rsapi, resrc, resrc_reqst, found_tree, true);
    printf ("%"PRId64" nodes available\n", nfound); // TEST
    if (synchronize (*found_tree)) {
        flux_log (h, LOG_ERR, "Error synchronizing");
        resrc_tree_destroy (rsapi, *found_tree, false, false);
        *found_tree = NULL;
        return 0;
    }

    /* A resource request's starttime is the most current time */
    /* We can't satisfy the request now */
    if (nfound < resrc_reqst_reqrd_qty (resrc_reqst)) {
        /* A little time saver; if you're not going to reserve, don't bother
         * trying and failing to select_resources
         */
        if (reservation_depth == 0 ||
                curr_reservation_depth >= reservation_depth ||
                nfound == 0) {
            resrc_tree_destroy (rsapi, *found_tree, false, false);
            *found_tree = NULL;
            return 0;
        }
        /* TODO: Also set a flag that select_resources can see so it doesn't try
         * to place the job now; it will only try to make a reservation */
    }
    return nfound;
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
        if (select_resources_in_time (h, rsapi, child_tree,
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

static int sort_ascending (void *key1, void *key2)
{
    return strcmp ((const char *)key1, (const char *)key2);
}

//static int sort_descending (void *key1, void *key2)
//{
//    return -1 * strcmp ((const char *)key1, (const char *)key2);
//}

/* Get switches with at least n_nodes available. Adds to a hash table with the
 * following format for keys <nodes_available><switch_name> e.g. 0015l2_switch4
 * This is done so they can be sorted by available nodes. If no such switches
 * are available, returns NULL. Assumes the input tree is rooted at a pod.
 */
zhash_t *get_switches (topo_tree_t *tt_pod, int64_t nodes_requested)
{
    int64_t nodes_avail;
    zhash_t *switch_h = zhash_new ();
    zlist_t *switches = tt_pod->children;
    topo_tree_t *sw = zlist_first (switches);
    char *key;
    while (sw != NULL) {
        nodes_avail = (levels[tree_depth-1]) -
                (sw->a_tier.T1 + sw->r_tier.T1 +
                 sw->a_tier.T2 + sw->r_tier.T2 +
                 sw->a_tier.T3 + sw->r_tier.T3);
        if (nodes_avail >= nodes_requested) {
            key = xasprintf ("%020"PRId64"%s",
                    nodes_avail, resrc_name (resrc_tree_resrc (sw->ct)));
            zhash_insert (switch_h, key, sw);
            zhash_freefn (switch_h, key, NULL); // We don't want to free tt_pod
            // printf ("Added key '%s'\n", key); // TEST
            free (key);
        }
        sw = zlist_next (switches);
    }
    return switch_h;
}

/* Get pods with at least n_nodes available. Adds to a hash table with the
 * following format for keys <nodes_available><switch_name> e.g. 0000315pod2
 * This is done so they can be sorted by available nodes. If no such switches
 * are available, returns NULL.
 */
zhash_t *get_pods (topo_tree_t *tt_root, int64_t nodes_requested)
{
    int64_t nodes_avail;
    zhash_t *pod_h = zhash_new ();
    zlist_t *pods = tt_root->children;
    topo_tree_t *pd = zlist_first (pods);

    char *key;
    /* Step 1: For each pod, calculate the number of available nodes */
    while (pd != NULL) {
        nodes_avail = (levels[tree_depth-1] * levels[tree_depth-2]) -
                (pd->a_tier.T1 + pd->r_tier.T1 +
                 pd->a_tier.T2 + pd->r_tier.T2 +
                 pd->a_tier.T3 + pd->r_tier.T3);
        if (nodes_avail >= nodes_requested) {
            key = xasprintf ("%020"PRId64"%s",
                    nodes_avail, resrc_name (resrc_tree_resrc (pd->ct)));
            zhash_insert (pod_h, key, pd);
            zhash_freefn (pod_h, key, NULL); // We don't want to free tt_root
            // printf ("Added key '%s'\n", key);
            free (key);
        }
        pd = zlist_next (pods);
    }
    return pod_h;
}

/* Checks if the node is available right now */
int static inline is_available (topo_tree_t *node)
{
    return ! (node->a_tier.T1 | node->a_tier.T2 | node->a_tier.T3
              | node->r_tier.T1 | node->r_tier.T2 | node->r_tier.T3);
}

/* Selects at most n_nodes in a given switch, appending them to selected_nodes.
 * Returns the number of nodes added to the list.
 */
int64_t select_nodes (
        topo_tree_t *a_switch, int64_t n_nodes, zlist_t **selected_nodes)
{
    if (*selected_nodes == NULL) {
        *selected_nodes = zlist_new ();
    }
    int64_t nodes_added = 0;
    zlist_t *candidate_nodes = a_switch->children;
    topo_tree_t *node = zlist_first (candidate_nodes);
    while (nodes_added < n_nodes && node != NULL) {
        if (is_available (node)) {
            zlist_append (*selected_nodes, node);
            // printf("Added node %s to selection\n",
            //         resrc_name (resrc_tree_resrc (node->ct))); // TEST
            nodes_added++;
        }
        node = zlist_next (candidate_nodes);
    }
    return nodes_added;
}

/*
 * case t1:
 *      from (LAP:MAP such that N_avail >= N)
 *          from (LAS:MAS such that N_avail >= N)
 *              for (nd in Nodes)
 *                  if (available) assign and update and increment nfound
 *                  if (nfound == N) we're done
 */
/* Selects a set of nodes. Expects pods and switches to have at least n_nodes
 * nodes available. The preference is the fullest switch which can still hold
 * that job (a best-fit approach). Returns a list of n_nodes nodes or NULL if
 * the request cannot be satisfied.
 */
zlist_t *t1_job_select (topo_tree_t *tt, int64_t n_nodes)
{
    zhash_t *pods = get_pods (tt, n_nodes);
    zlist_t *sorted_pod_keys = zhash_keys (pods);
    zlist_sort (sorted_pod_keys, sort_ascending);

    char *pod_key = zlist_first (sorted_pod_keys);
    topo_tree_t *a_pod;

    zhash_t *switches;
    zlist_t *sorted_switch_keys;
    char *switch_key;
    topo_tree_t *a_switch;
    int64_t nodes_found = 0;

    zlist_t *selected_nodes = zlist_new ();

    /* Loop over all pods */
    while (pod_key != NULL) {
        /*  get_switches will only return the switches with more than n_nodes
         *  available so we don't need to loop over sorted_switch_keys. */
        a_pod = zhash_lookup (pods, pod_key);
        switches = get_switches (a_pod, n_nodes);
        sorted_switch_keys = zhash_keys (switches);
        if (zlist_size (sorted_switch_keys) == 0) {
            pod_key = zlist_next (sorted_pod_keys);
            continue;
        }
        zlist_sort (sorted_switch_keys, sort_ascending);
        switch_key = zlist_first (sorted_switch_keys);
        a_switch = zhash_lookup (switches, switch_key);
        printf ("Trying with pod %s and switch %s:\n", pod_key, switch_key); // TEST
        nodes_found = select_nodes (a_switch, n_nodes, &selected_nodes);
        if (nodes_found == n_nodes) {
            zhash_destroy (&switches);
            zlist_destroy (&sorted_switch_keys);
            break;
        } else {
            zhash_destroy (&switches);
            zlist_destroy (&sorted_switch_keys);
            zlist_destroy (&selected_nodes);
            pod_key = zlist_next (sorted_pod_keys);
        }
    }
    if (nodes_found != n_nodes) {
        printf("Found %"PRId64" of %"PRId64" nodes\n", nodes_found, n_nodes); // TEST
        zlist_destroy (&selected_nodes);
        selected_nodes = NULL;
    }
    zhash_destroy (&pods);
    zlist_destroy (&sorted_pod_keys);
    return selected_nodes;
}

/*
 * case t2:
 *      from (LAP:MAP such that N_avail >= N)
 *          AS = {s | T(s) < T2}
 *          from (MAS:LAS in AS)
 *              for (nd in Nodes)
 *                  if (available) assign and update and increment nfound
 *                  if (nfound == N) we're done
 */
zlist_t *t2_job_select (topo_tree_t *tt, int64_t n_nodes)
{
    return NULL;
}

/*
 * case t3:
 *      AP = {p | T(p) < T3}
 *      from (p = MAP:LAP in AP)
 *          AS = {s | T(s) < T2 in p}
 *          from (MAS:LAS in AS)
 *              for (nd in Nodes)
 *                  if (available) assign and update and increment nfound
 *                  if (nfound == N) we're done
 */
zlist_t *t3_job_select (topo_tree_t *tt, int64_t n_nodes)
{
    return NULL;
}

/* Finds you n_nodes nodes, if possible, given the topology tree of available
 * and reserved nodes. Returns a list of nodes if the request could be satisfied
 * and NULL otherwise. For a higher-level overview, see Overview
 */
zlist_t *topo_select_resources (topo_tree_t *tt, int64_t n_nodes)
{
    int tier = calculate_tier (n_nodes);
    printf ("Searching for %"PRId64" nodes (T%d)\n", n_nodes, tier); // TEST
    // topo_tree_print (tt); // TEST
    switch (tier) {
    case 1:
        return t1_job_select (tt, n_nodes);
    case 2:
        return t2_job_select (tt, n_nodes);
    case 3:
        return t3_job_select (tt, n_nodes);
    default:
        return NULL; // This should never happen
    }
} 

/* Converts a list of nodes into a valid resource request fulfillment, by
 * returning a tree of selected resources and, if ALLOCATE, setting all the
 * resources in resrc_request to found.
 */
resrc_tree_t *convert (
        zlist_t *selected_nodes, resrc_reqst_t *resrc_reqst, resrc_enum_t which)
{
    return NULL;
}

/*
 * select_resources() selects from the set of resource candidates the best
 * resources for the job. If the resources cannot be found now, then look into
 * the future using each job completion time to see if you can reserve the job.
 *
 * Inputs:  found_tree      - tree of potential resources
 *          resrc_reqst     - the resources the job requests. This function may
 *                            change the request's n_found, start, and end times
 *          selected_parent - parent of the selected resource tree
 * Returns: a resource tree of however many resources were selected
 */
resrc_tree_t *select_resources (flux_t *h, resrc_api_ctx_t *rsapi,
                                resrc_tree_t *found_tree,
                                resrc_reqst_t *resrc_reqst,
                                resrc_tree_t *selected_parent)
{
    if (!resrc_reqst) {
        flux_log (h, LOG_ERR, "%s: called with empty request", __FUNCTION__);
        return NULL;
    }
    resrc_tree_t *selected_tree = NULL;
    zlist_t *selected_nodes;
    /* XXX: Do I want resrc_phys_tree or found_tree? */
    resrc_tree_t *root = resrc_phys_tree (resrc_tree_resrc (found_tree));
    int64_t current_time = resrc_reqst_starttime (resrc_reqst);
    int64_t req_walltime = resrc_reqst_endtime (resrc_reqst) - current_time;

    /* Search for resources now */
    selected_nodes = topo_select_resources (
            topo_tree, resrc_reqst_reqrd_qty (resrc_reqst));
    if (selected_nodes == NULL) {
        flux_log (h, LOG_DEBUG, "Not enough resources available now");
    } else {
        current_selected_nodes = selected_nodes;
        selected_tree = convert (selected_nodes, resrc_reqst, ALLOCATE);
        flux_log (h, LOG_DEBUG, "%s: found all resources at current time "
                "%"PRId64")", __FUNCTION__, current_time);
        // return selected_tree; TOPOTODO replace UNNECESSARY with this line.
    }
    /* TOPOTODO: This will be UNNECESSARY once the topology scheduler is finished */
    selected_tree = select_resources_in_time (
         h, rsapi, root, resrc_reqst, NULL);

    if (resrc_reqst_all_found (resrc_reqst)) {
        flux_log (h, LOG_DEBUG, "%s: found all resources at current time "
                "%"PRId64")", __FUNCTION__, current_time);
        return selected_tree;
    }
    /* Reset any partial selections we made looking at the current time */
    resrc_reqst_clear_found (resrc_reqst);
    resrc_tree_unstage_resources (root);
    /* End UNNECESSARY */

    /* Search for resources in the future */
    int64_t *c_time = NULL;
    int64_t prev_c_time = -1;
    zlist_sort (all_completion_times, compare_int64_ascending);
    /* TOPOTODO Make a copy of the tree */
    zhash_t *future_node2topo = zhash_new ();
    topo_tree_t *future_tree = topo_tree_copy (topo_tree, future_node2topo);
    zlist_sort (job_allocation_list, &compare_allocation_ascending);
    printf ("job_alloc_list has %ld elements, sorted\n",
            zlist_size (job_allocation_list)); // TEST
    allocation_t *alloc;
    for (alloc = zlist_first (job_allocation_list);
         alloc;
         alloc = zlist_next (job_allocation_list)) {
        if (alloc->completion_time < current_time) {
            zlist_remove (job_allocation_list, alloc);
            printf ("Removing job %"PRId64" from list at %"PRId64"\n",
                    alloc->job_id, current_time); // TEST
            continue;
        }
        /* TOPODO: Do the magic */
    }

    for (c_time = zlist_first (all_completion_times);
         c_time;
         c_time = zlist_next (all_completion_times)) {
        /* Purge past times from consideration */
        if (*c_time < current_time) {
            zlist_remove (all_completion_times, c_time);
            continue;
        }
        /* TOPOTODO: If 2 jobs end at the same time we want to test em both, so
         * remove this next bit. */
        /* Don't test the same time multiple times */
        if (prev_c_time == *c_time) {
            continue;
        }
        flux_log (h, LOG_DEBUG, "Attempting to find %"PRId64" nodes "
                  "at time %"PRId64"",
                  resrc_reqst_reqrd_qty (resrc_reqst), *c_time + 1);
        /* TOPOTODO Get the job associated with the most recent completion time */
        /* TOPOTODO Free those nodes in the topo_tree_copy */
        /* TOPOTODO This */
        selected_nodes = topo_select_resources (
                future_tree, resrc_reqst_reqrd_qty (resrc_reqst));
        if (selected_nodes == NULL) {
            flux_log (h, LOG_DEBUG, "Topo could not reserve enough resources at"
                    " time % "PRId64"\n", *c_time + 1);
        } else {
            current_selected_nodes = selected_nodes;
            selected_tree = convert (selected_nodes, resrc_reqst, RESERVE);
            zhash_destroy (&future_node2topo);
            topo_tree_destroy (future_tree);
            return selected_tree;
        }
        /* UNNECESSARY once topo scheduler is working */
        resrc_reqst_set_starttime (resrc_reqst, *c_time + 1);
        resrc_reqst_set_endtime (resrc_reqst, *c_time + 1 + req_walltime);
        selected_tree = select_resources_in_time (
                h, rsapi, root, resrc_reqst, NULL);
        /* If you found them all, great! But it's in the future, so you have to
         * clear all found resources to ensure reserve_resources is called */
        if (resrc_reqst_all_found (resrc_reqst)) {
            resrc_reqst_clear_found (resrc_reqst);
            resrc_tree_unstage_resources (root);
            return selected_tree;
        }
        /* Otherwise, just wipe out the treea and clear the incomplete set of
         * resources we may have found and start over at a later time. */
        resrc_tree_destroy (rsapi, selected_tree, false, false);
        resrc_reqst_clear_found (resrc_reqst);
        resrc_tree_unstage_resources (root);
        prev_c_time = *c_time;
        /* End UNNECESSARY */
    }
    flux_log (h, LOG_DEBUG, "This request can never be satisfied");
    return NULL;
}

/* This will select some resources for a given job, either now or in the future
 */
static resrc_tree_t *select_resources_in_time (flux_t *h,
                                               resrc_api_ctx_t *rsapi,
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
            if (select_resources_in_time (h, rsapi, child_tree,
                    resrc_reqst, selected_tree) &&
                resrc_reqst_nfound (resrc_reqst) >=
                resrc_reqst_reqrd_qty (resrc_reqst))
                break;
            child_tree = resrc_tree_list_next (children);
        }
    }

    return selected_tree;
}

/* Find the nodes (physical compute nodes) of a tree.
 * Expects an initial argument of an empty list and the root of the tree you
 * wish to build. Don't forget to call resrc_tree_list_shallow_destroy once
 * you're done with this.
 */
static void find_nodes (resrc_tree_t *r_tree, resrc_tree_list_t *rtl)
{
    resrc_t *resrc = resrc_tree_resrc (r_tree);
    if (strcmp (resrc_type (resrc), "node") == 0) {
        // printf ("found node '%s'\n", resrc_name (resrc)); // TEST
        resrc_tree_list_append (rtl, r_tree);
    }
    resrc_tree_list_t *children = resrc_tree_children (r_tree);
    if (children == NULL) {
        return;
    }
    resrc_tree_t *child = resrc_tree_list_first (children);
    while (child != NULL) {
        find_nodes (child, rtl);
        child = resrc_tree_list_next (children);
    }
}

/* Marks the topology tree by tiers for each resource in the selected_tree
 * This counts the number of nodes requested in the selected tree then
 * increments the counters for the node, switch, and pod.
 */
static void mark_topo_tree (
        flux_t *h, resrc_tree_t *selected_tree, mark_enum_t which)
{
    /* 1. Count number of nodes to determine the tier of the job */
    resrc_tree_list_t *node_list = resrc_tree_list_new ();
    find_nodes (selected_tree, node_list);
    int64_t n_nodes = resrc_tree_list_size (node_list);
    int job_tier = calculate_tier (n_nodes);
    // printf ("Selected %"PRId64" nodes, T%d job\n", n_nodes, job_tier); // TEST
    /* 2. Look up the nodes in the hash table and propagate tier counts up */
    resrc_tree_t *node = resrc_tree_list_first (node_list);
    topo_tree_t *topo_node;
    char *node_name;
    while (node != NULL) {
        node_name = resrc_name (resrc_tree_resrc (node));
        topo_node = zhash_lookup (node2topo_hash, node_name);
        if (topo_node == NULL) {
            flux_log (h, LOG_ERR, "Could not find node '%s'", node_name);
        }
        node = resrc_tree_list_next (node_list);
        if (which == RESERVE) {
            // resrc_tree_print (selected_tree); // TEST
            // printf ("Reserving '%s'\n", node_name);
            if (job_tier == 1) {
                topo_node->r_tier.T1++;
                topo_node->parent->r_tier.T1++;
                topo_node->parent->parent->r_tier.T1++;
                // printf ("A T1 job r_marked on node '%s'\n", node_name); // TEST
            } else if (job_tier == 2) {
                topo_node->r_tier.T2++;
                topo_node->parent->r_tier.T2++;
                topo_node->parent->parent->r_tier.T2++;
                // printf ("A T2 job r_marked on node '%s'\n", node_name); // TEST
            } else { // job_tier == 3
                topo_node->r_tier.T3++;
                topo_node->parent->r_tier.T3++;
                topo_node->parent->parent->r_tier.T3++;
                // printf ("A T3 job r_marked on node '%s'\n", node_name); // TEST
            }
        } else { // ALLOCATE
            if (job_tier == 1) {
                topo_node->a_tier.T1++;
                topo_node->parent->a_tier.T1++;
                topo_node->parent->parent->a_tier.T1++;
                // printf ("A T1 job a_marked on node '%s'\n", node_name); // TEST
            } else if (job_tier == 2) {
                topo_node->a_tier.T2++;
                topo_node->parent->a_tier.T2++;
                topo_node->parent->parent->a_tier.T2++;
                // printf ("A T2 job a_marked on node '%s'\n", node_name); // TEST
            } else { // job_tier == 3
                topo_node->a_tier.T3++;
                topo_node->parent->a_tier.T3++;
                topo_node->parent->parent->a_tier.T3++;
                // printf ("A T3 job a_marked on node '%s'\n", node_name); // TEST
            }
        }
    }
    resrc_tree_list_shallow_destroy (node_list);
}

/* Once the resources have been found and selected, mark them in the selected
 * tree as allocated.
 */
int allocate_resources (flux_t *h, resrc_api_ctx_t *rsapi,
                        resrc_tree_t *selected_tree, int64_t job_id,
                        int64_t starttime, int64_t endtime)
{
    int rc = -1;
    if (selected_tree) {
        rc = resrc_tree_allocate (selected_tree, job_id, starttime, endtime);
        if (!rc) {
            // mark_topo_tree (h, current_selected_nodes, RESERVE);
            mark_topo_tree (h, selected_tree, ALLOCATE);
            int64_t *completion_time = xzmalloc (sizeof(int64_t));
            *completion_time = endtime;
            rc = zlist_append (allocation_completion_times, completion_time);
            zlist_freefn (allocation_completion_times, completion_time, free, true);
            rc |= zlist_append (all_completion_times, completion_time);
            flux_log (h, LOG_DEBUG, "Allocated job %"PRId64" from %"PRId64" to "
                      "%"PRId64"", job_id, starttime, *completion_time);
            /* Save off the allocation */
            allocation_t *allocation = xzmalloc (sizeof(allocation_t));
            if (allocation == NULL) {
                flux_log (h, LOG_ERR, "Unable to allocate allocation");
                return -1;
            }
            allocation->completion_time = endtime;
            allocation->job_id = job_id;
            allocation->selected_nodes = current_selected_nodes;
            current_selected_nodes = NULL; // So it doesn't get destroyed
            zlist_append (job_allocation_list, allocation);
            zlist_freefn (job_allocation_list, allocation, free, true);
            printf("Added job %"PRId64" ending @ %"PRId64" to list, (size %ld)\n",
                    job_id, endtime, zlist_size (job_allocation_list)); // TEST
        }
    }
    return rc;
}

/*
 * reserve_resources() reserves resources for the specified job id.
 * Reservation is using EASY backfill; only the first job in the queue will
 * get a reservation. This assumes the selected tree is correct if non-null.
 * Unused: starttime (the more updated one is in resrc_reqst)
 *         resrc
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
            /* TOPOTODO: Does this interfere since it's not a allocation? */
            // mark_topo_tree (h, current_selected_nodes, RESERVE);
            mark_topo_tree (h, *selected_tree, RESERVE); // TODO: Not working
            curr_reservation_depth++;
            int64_t *completion_time = xzmalloc (sizeof(int64_t));
            *completion_time = reservation_completion_time;
            rc = zlist_append (all_completion_times, completion_time);
            zlist_freefn (all_completion_times, completion_time, free, true);
            flux_log (h, LOG_DEBUG, "Reserved %"PRId64" nodes for job "
                      "%"PRId64" from %"PRId64" to %"PRId64"",
                      resrc_reqst_reqrd_qty (resrc_reqst), job_id,
                      reservation_starttime + 1,
                      reservation_starttime + 1 + walltime);
            printf ("resrc_reqst has %"PRId64" reserved resources\n",
                    resrc_reqst_nfound (resrc_reqst)); // TEST
        }
        if (current_selected_nodes != NULL) {
            zlist_destroy (&current_selected_nodes);
            current_selected_nodes = NULL;
        }
    }
    return rc;
}

/* Helper function for allocate_topo_structures which takes as input the root
 * of a tree and counts the number of resources at a given level l.
 */
static int64_t resources_at_level (resrc_tree_t *tree, int l)
{
    // TODO: Update this to go through each level and actually count them
    int idx;
    int64_t count = 1;
    for (idx = 0; idx <= l; idx++) {
        count *= levels[idx]; 
    }
    return count;
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
    char topo_filename[PATH_MAX];
    topo_filename[0] = '\0'; /* Copy the existing ctx by default */
    topo_rsapi = resrc_api_init ();

    for (entry = argz;
         entry;
         entry = argz_next (argz, argz_len, entry)) {

        if (!strncmp ("reserve-depth=", entry, sizeof ("reserve-depth"))) {
            reserve_depth_str = strstr (entry, "=") + 1;
        } else if (!strncmp ("rdl-topology=", entry, sizeof("rdl-topology"))) {
            strncpy (topo_filename, strstr (entry, "=") + 1, PATH_MAX);
        } else {
            flux_log(h, LOG_ERR, "Invalid argument %s", entry);
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

    if (topo_filename[0] != '\0') {
        if (rsreader_resrc_bulkload (topo_rsapi, topo_filename, NULL)) {
            flux_log (h, LOG_ERR, "Unable to read rdl-topology file '%s'",
                    topo_filename);
            rc = -1;
            goto done;
        }
        if (allocate_topo_structures (h)) {
            flux_log (h, LOG_ERR, "Unable to allocate topology data structures");
            rc = -1;
            goto done;
        }
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

 done:
    return rc;
}

/* Go through the topology resource tree and allocate an array for each
 * level to contain which resources are allocated and when
 */
static int allocate_topo_structures (flux_t *h)
{
    tree_depth = 0;
    /* Pass 1: Count the depth of the tree */
    resrc_tree_t *tree = resrc_tree_root (topo_rsapi);
    resrc_tree_list_t *children = resrc_tree_children (tree);
    resrc_t *resrc = resrc_tree_resrc (tree);
    // resrc_tree_print (tree);
    // There are other resources but we stop at the node level
    while (strcmp (resrc_type (resrc), "node")) {
        printf ("Type at level %d is %s\n", tree_depth, resrc_type (resrc)); // TEST
        tree_depth++;
        tree = resrc_tree_list_first (children);
        children = resrc_tree_children (tree);
        resrc = resrc_tree_resrc (tree);
        if (resrc == NULL) {
            flux_log(h, LOG_ERR, "Got to level %d without finding 'node' type",
                    tree_depth);
            return -1;
        }
    }
    printf ("Tree depth = %d\n", tree_depth); // TEST
    levels = xzmalloc (tree_depth * sizeof(int64_t));
    if (levels == NULL) {
        return -1;
    }

    /* Pass 2: Allocate availability arrays for each level */
    int d;
    int64_t t_d;
    tree = resrc_tree_root (topo_rsapi);
    children = resrc_tree_children (tree);
    for (d = 0; d < tree_depth; d++) {
        levels[d] = resrc_tree_num_children (tree);
        t_d = resources_at_level (tree, d);
        tree = resrc_tree_list_first (children);
        children = resrc_tree_children (tree);
        printf ("Tree fan-out at level %d = %"PRId64", total sz = %"PRId64"\n",
                d, levels[d], t_d); // TEST
    }

    /* XXX: If the fan-out isn't symmetric across all leaves, we should
     * identify those that are smaller then set them as always allocated,
     * t1 objects (can play nice with everyone) via 1 << 63
     */
    return 0;
}

// /* Destructor of sorts; releases all the data structures we used
//  * XXX: Call this at the end */
// int finalize (flux_t *h)
// {
//     resrc_api_fini (topo_rsapi);
//     free (levels);
//     topo_tree_destroy (topo_tree);
//     zhash_destroy (&node2topo_hash);
//     zlist_destroy (&job_allocation_list);
// }

MOD_NAME ("sched.topo");

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

