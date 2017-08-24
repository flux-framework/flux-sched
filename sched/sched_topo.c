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

/* Overview
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
 * We distinguish between three types of jobs. Let
 *   N = number of nodes requested
 *   levels[i] be the number of children of a vertex at level Li
 * T1: (N < levels[2]) A job fitting within a single L2 tree ("switch")
 * T2: (levels[1] < N <= levels[1]*levels[2]) A job spanning multiple L2
 *      trees but still within one L1 tree ("pod")
 * T3: (N > levels[1]*levels[2]) A job spanning multiple L1 trees ("pods")
 * 
 * Without topology-aware scheduling, effectively every job is treated as
 * a T1 job; they may coexist in any way. Thus we do not need to use auxiliary
 * data structures for T1 jobs and can bypass the topology-specific checks.
 *
 * Larger jobs pose more challenges; we must keep track of which L2 and L1
 * trees are currently occupied or reserved, and by which type of job.
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
 * 
 * Auxiliary Data Structures and notes from meeting:
 * Tree which gets updated upon each job completing each job--hack sched.c
 * to call plugin->job_completed.
 * This will be a tree with the following structs
 * struct {
 *      ptr to resource tree
 *      job_type (0 - avail, 1,2,3 -> T1,T2,T3)
 *      children (ptr to list of topotree)
 *      resrc_t  (node, switch, pod, cluster)
 * } topotree;
 *
 * Then the algorithm, where
 * L = least, A = available, P = pod, S = switch, N = number of nodes in a job
 *
 * count = 0
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
 * Unresolved issues:
 * Can the plugin assume it is loaded when no jobs are running?
 * If not, then we need to set the tiers upon initialization
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

static int reservation_depth = 1;
static int curr_reservation_depth = 0;
static zlist_t *all_completion_times = NULL;
/* Topology-specific data structures and types */
typedef enum {RESERVE, ALLOCATE} mark_enum_t;
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
 * Also creates a hash for fast lookup from resrc name -> topo */
static topo_tree_t *create_topo_tree (
        flux_t *h, resrc_tree_t *tree, topo_tree_t *parent)
{
    int rc;
    topo_tree_t *new_tree;
    char *r_t = resrc_type (resrc_tree_resrc (tree));
    resrc_enum_t r_e = resrc_enum_from_type (r_t);

    int lvl = -1;
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
    new_tree->parent = parent;
    new_tree->children = NULL;
    
    if (strcmp (r_t, "node") == 0) {
        rc = zhash_insert (node2topo_hash,
                           resrc_name (resrc_tree_resrc (tree)),
                           new_tree);
        if (rc) {
            flux_log (h, LOG_ERR, "Error adding '%s' to hash\n",
                    resrc_name (resrc_tree_resrc (tree)));
            return NULL;
        }
        // printf ("added '%s' to hash\n", resrc_name (resrc_tree_resrc (tree))); // TEST
    }

    /* Loop over all children and only add the non-unknown ones */
    topo_tree_t *topo_child;
    resrc_tree_list_t *children = resrc_tree_children (tree);
    resrc_tree_t *child = resrc_tree_list_first (children);
    while (child != NULL) {
        topo_child = create_topo_tree (h, child, new_tree);
        if (topo_child != NULL) {
            if (new_tree->children == NULL) {
                new_tree->children = zlist_new ();
            }
            rc = zlist_append (new_tree->children, new_tree);
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

/* Removes all existing reservations (r_tier) from the topology tree */
static void purge_reservations (topo_tree_t *tt)
{
    if (tt == NULL) {
        return;
    }
   tt->r_tier = initialize_tier_counts ();
   // zlist_t *children = topo_tree->children;
   // if (children == NULL)
   //     return;
   // topo_tree_t *child = zlist_first (children);
   // while (child != NULL) {
   //     purge_reservations (child);
   //     child = zlist_next (children);
   // }
}

/* Update the topology tree with the current state of the resource tree.
 * This will only decrement tier counters in the topology tree, not increment
 * them. Incrementing is done by mark_topo_tree.
 * Returns 0 if okay, -1 if something bad happened.
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
            // printf ("A T1 job completed on node '%s'\n", node_name); // TEST
        } else if (tt->a_tier.T2 > 0) {
            tt->a_tier.T2--;
            tt->parent->a_tier.T2--;
            // printf ("A T2 job completed on node '%s'\n", node_name); // TEST
        } else if (tt->a_tier.T3 > 0) {
            tt->a_tier.T3--;
            tt->parent->a_tier.T3--;
            tt->parent->parent->a_tier.T3--;
            // printf ("A T3 job completed on node '%s'\n", node_name); // TEST
        }
    } else { /* Recurse */
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
 * before this. Allocates structures assuming no nodes are allocated.
 */
int sched_loop_setup (flux_t *h, resrc_api_ctx_t *rsapi)
{
    curr_reservation_depth = 0;
    if (!all_completion_times)
        all_completion_times = zlist_new ();

    if (topo_tree == NULL) {
        resrc_tree_t *root = resrc_tree_root (rsapi);
        if (root) {
            node2topo_hash = zhash_new ();
            topo_tree = create_topo_tree (h, root, NULL);
        } else {
            flux_log (h, LOG_ERR, "rsapi->tree_root is NULL");
            return -1;
        }
    } else {
        purge_reservations (topo_tree);
    }
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
 * find_resources() identifies the all of the resource candidates for
 * the job.  The set of resources returned could be more than the job
 * requires.  A later call to select_resources() will cull this list
 * down to the most appropriate set for the job.
 *
 * Inputs:  rsapi       - Contains the root of the resource tree
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

    if (!resrc || !resrc_reqst) {
        flux_log (h, LOG_ERR, "%s: invalid arguments", __FUNCTION__);
        return 0;
    }
    
    /* A resource request's starttime is the most current time */
    int64_t current_time = resrc_reqst_starttime (resrc_reqst);
    if (current_time < 0) {
        printf ("Current time = %"PRId64"\n", current_time); // TEST
    }
    *found_tree = NULL;
    nfound = resrc_tree_search (rsapi, resrc, resrc_reqst, found_tree, true);

    if (!nfound && *found_tree) {
        resrc_tree_destroy (rsapi, *found_tree, false, false);
        *found_tree = NULL;
    } else {
        /* Only synchronize if we found some resources available */
        /* TODO: Keep in mind that resrc_tree_search will only match resources
         * with the same properties and tags, but there may be resources which
         * are different enough to not match but still can affect the job from
         * an interference perspective.
         * At least in simulation, resrc_tree_allocate only calls
         * resrc_tree_allocate_in_time, which never sets the resrc state
         */
        printf ("%"PRId64" nodes available\n", nfound); // TEST
        if (synchronize (*found_tree)) {
            flux_log (h, LOG_ERR, "Error synchronizing");
            return 0;
        }
    }

    // A little time saver; if you're not going to reserve, don't bother
    // trying and failing to select_resources
    if (reservation_depth == 0 || curr_reservation_depth >= reservation_depth) {
        return 0;
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
        if (select_resources (h, rsapi, child_tree,
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
 * select_resources() selects from the set of resource candidates the
 * best resources for the job.
 *
 * Inputs:  found_tree      - tree of resource tree candidates
 *          resrc_reqst     - the resources the job requests
 *          selected_parent - parent of the selected resource tree
 * Returns: a resource tree of however many resources were selected
 */
resrc_tree_t *select_resources (flux_t *h, resrc_api_ctx_t *rsapi,
                                resrc_tree_t *found_tree,
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

    /* TODO: Need a way to go up. What's happening is you go from
     * pod -> switch -> node, then reserve, but you only get one then you have
     * to do it over again. You're updating the resrc_reqst but not
     * selected_tree.
     */
    // size_t n_required = resrc_reqst_reqrd_qty (resrc_reqst);
    // int tier = calculate_tier (n_required);
    // printf ("You're trying to select a T%d job\n", tier); // Gets called lots

    resrc = resrc_tree_resrc (found_tree);
    if (resrc_match_resource (resrc, resrc_reqst, true)) { /* If correct type */
        if (resrc_reqst_num_children (resrc_reqst)) { /* And has children */
            if (resrc_tree_num_children (found_tree)) {
                selected_tree = resrc_tree_new (selected_parent, resrc);
                if (select_children (h, rsapi, resrc_tree_children (found_tree),
                                     resrc_reqst_children (resrc_reqst),
                                     selected_tree)) {
                    resrc_stage_resrc (resrc,
                                       resrc_reqst_reqrd_size (resrc_reqst),
                                       resrc_reqst_graph_reqs (resrc_reqst));
                    resrc_reqst_add_found (resrc_reqst, 1);
                    flux_log (h, LOG_DEBUG, "selected %s", resrc_name (resrc));
                } else {
                    resrc_tree_destroy (rsapi, selected_tree, false, false);
                }
            }
        } else {
            selected_tree = resrc_tree_new (selected_parent, resrc);
            resrc_stage_resrc (resrc, resrc_reqst_reqrd_size (resrc_reqst),
                                       resrc_reqst_graph_reqs (resrc_reqst));
            resrc_reqst_add_found (resrc_reqst, 1);
            flux_log (h, LOG_DEBUG, "selected %s", resrc_name (resrc));
        }
    } else if (resrc_tree_num_children (found_tree)) {
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
        children = resrc_tree_children (found_tree);
        child_tree = resrc_tree_list_first (children);
        while (child_tree) {
            if (select_resources (h, rsapi, child_tree,
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
 * increments based on the following logic:
 * T1: Increment just that node
 * T2: Increment that node and its parent (switch)
 * T3: Increment that node, its parent (switch), and its parent's parent (pod)
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
                printf ("A T1 job r_marked on node '%s'\n", node_name); // TEST
            } else if (job_tier == 2) {
                topo_node->r_tier.T2++;
                topo_node->parent->r_tier.T2++;
                printf ("A T2 job r_marked on node '%s'\n", node_name); // TEST
            } else { // job_tier == 3
                topo_node->r_tier.T3++;
                topo_node->parent->r_tier.T3++;
                topo_node->parent->parent->r_tier.T3++;
                printf ("A T3 job r_marked on node '%s'\n", node_name); // TEST
            }
        } else { // ALLOCATE
            if (job_tier == 1) {
                topo_node->a_tier.T1++;
                // printf ("A T1 job a_marked on node '%s'\n", node_name); // TEST
            } else if (job_tier == 2) {
                topo_node->a_tier.T2++;
                topo_node->parent->a_tier.T2++;
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
 * tree as allocated
 */
int allocate_resources (flux_t *h, resrc_api_ctx_t *rsapi,
                        resrc_tree_t *selected_tree, int64_t job_id,
                        int64_t starttime, int64_t endtime)
{
    int rc = -1;
    if (selected_tree) {
        rc = resrc_tree_allocate (selected_tree, job_id, starttime, endtime);
        if (!rc) {
            mark_topo_tree (h, selected_tree, ALLOCATE);
            int64_t *completion_time = xzmalloc (sizeof(int64_t));
            *completion_time = endtime;
            rc = zlist_append (all_completion_times, completion_time);
            zlist_freefn (all_completion_times, completion_time, free, true);
            flux_log (h, LOG_DEBUG, "Allocated job %"PRId64" from %"PRId64" to "
                      "%"PRId64"", job_id, starttime, *completion_time);
        }
    }
    return rc;
}

/*
 * reserve_resources() reserves resources for the specified job id.
 * Unlike the FCFS version where selected_tree provides the tree of
 * resources to reserve, this backfill version will search into the
 * future to find a time window when all of the required resources are
 * available, reserve those, and return the pointer to the selected
 * tree.
 */
int reserve_resources (flux_t *h, resrc_api_ctx_t *rsapi,
                       resrc_tree_t **selected_tree, int64_t job_id,
                       int64_t starttime, int64_t walltime, resrc_t *resrc,
                       resrc_reqst_t *resrc_reqst)
{
    int rc = -1;
    int64_t *completion_time = NULL;
    int64_t nfound = 0;
    int64_t prev_completion_time = -1;
    resrc_tree_t *found_tree = NULL;

    if (!resrc || !resrc_reqst) {
        flux_log (h, LOG_ERR, "%s: invalid arguments", __FUNCTION__);
        goto ret;
    }
    if (!reservation_depth)
        /* All backfilling (no reservations).  Return success to
         * backfill all jobs remaining in the queue */
        return 0;
    else if (reservation_depth == 1) {
        if (curr_reservation_depth)
            /* EASY Backfill.  Top priority job is reserved, so return
             * success to backfill all jobs remaining in the queue */
            return 0;
    } else if (curr_reservation_depth >= reservation_depth)
        /* Stop reserving and return -1 to stop scheduling any more jobs */
        goto ret;

    if (*selected_tree) {
        resrc_tree_destroy (rsapi, *selected_tree, false, false);
        *selected_tree = NULL;
    }
    zlist_sort (all_completion_times, compare_int64_ascending);

    for (completion_time = zlist_first (all_completion_times);
         completion_time;
         completion_time = zlist_next (all_completion_times)) {
        /* Purge past times from consideration */
        if (*completion_time < starttime) {
            zlist_remove (all_completion_times, completion_time);
            continue;
        }
        /* Don't test the same time multiple times */
        if (prev_completion_time == *completion_time)
            continue;

        resrc_reqst_set_starttime (resrc_reqst, *completion_time + 1);
        resrc_reqst_set_endtime (resrc_reqst, *completion_time + 1 + walltime);
        flux_log (h, LOG_DEBUG, "Attempting to reserve %"PRId64" nodes for job "
                  "%"PRId64" at time %"PRId64"",
                  resrc_reqst_reqrd_qty (resrc_reqst), job_id,
                  *completion_time + 1);

        nfound = resrc_tree_search (rsapi, resrc, resrc_reqst, &found_tree, true);
        if (nfound >= resrc_reqst_reqrd_qty (resrc_reqst)) {
            *selected_tree = select_resources (h, rsapi,
                                 found_tree, resrc_reqst, NULL);
            resrc_tree_destroy (rsapi, found_tree, false, false);
            if (*selected_tree) {
                rc = resrc_tree_reserve (*selected_tree, job_id,
                                         *completion_time + 1,
                                         *completion_time + 1 + walltime);
                if (rc) {
                    resrc_tree_destroy (rsapi, *selected_tree, false, false);
                    *selected_tree = NULL;
                } else {
                    mark_topo_tree (h, *selected_tree, RESERVE);
                    curr_reservation_depth++;
                    flux_log (h, LOG_DEBUG, "Reserved %"PRId64" nodes for job "
                              "%"PRId64" from %"PRId64" to %"PRId64"",
                              resrc_reqst_reqrd_qty (resrc_reqst), job_id,
                              *completion_time + 1,
                              *completion_time + 1 + walltime);
                }
                break;
            }
        }
        prev_completion_time = *completion_time;
    }
ret:
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
//     free(levels);
//     topo_tree_destroy (topo_tree); // UNIMPLEMENTED
//     zhash_destroy (&node2topo_hash);
// }

MOD_NAME ("sched.topo");

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

