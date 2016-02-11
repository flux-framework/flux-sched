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
#include <errno.h>
#include <libgen.h>
#include <czmq.h>
#include <json.h>
#include <flux/core.h>

#include "src/common/libutil/log.h"
#include "src/common/libutil/shortjson.h"
#include "src/common/libutil/xzmalloc.h"
#include "resrc.h"
#include "resrc_tree.h"
#include "resrc_reqst.h"
#include "schedsrv.h"


static bool select_children (flux_t h, resrc_tree_list_t *found_children,
                             resrc_reqst_list_t *reqst_children,
                             resrc_tree_t *parent_tree);

int sched_loop_setup (void)
{
    return 0;
}

/*
 * find_resources() identifies the all of the resource candidates for
 * the job.  The set of resources returned could be more than the job
 * requires.  A later call to select_resources() will cull this list
 * down to the most appropriate set for the job.
 *
 * Inputs:  resrcs - hash table of all resources
 *          resrc_reqst - the resources the job requests
 * Returns: a list of resource trees satisfying the job's request,
 *                   or NULL if none (or not enough) are found
 */
resrc_tree_list_t *find_resources (flux_t h, resrc_t *resrc,
                                   resrc_reqst_t *resrc_reqst)
{
    int64_t nfound = 0;
    resrc_tree_list_t *found_trees = NULL;
    resrc_tree_t *resrc_tree = NULL;

    if (!resrc || !resrc_reqst) {
        flux_log (h, LOG_ERR, "%s: invalid arguments", __FUNCTION__);
        goto ret;
    }
    resrc_tree = resrc_phys_tree (resrc);

    found_trees = resrc_tree_list_new ();
    if (!found_trees) {
        flux_log (h, LOG_ERR, "%s: new tree list failed", __FUNCTION__);
        goto ret;
    }

    if (resrc_reqst_set_starttime (resrc_reqst, 0) ||
        resrc_reqst_set_endtime (resrc_reqst, 0))
        goto ret;

    nfound = resrc_tree_search (resrc_tree_children (resrc_tree), resrc_reqst,
                                found_trees, true);

    if (!nfound) {
        resrc_tree_list_destroy (found_trees, false);
        found_trees = NULL;
    }
ret:
    return found_trees;
}

static bool select_child (flux_t h, resrc_tree_list_t *found_children,
                          resrc_reqst_t *child_reqst,
                          resrc_tree_t *parent_tree)
{
    resrc_tree_t *resrc_tree = NULL;
    resrc_tree_t *child_tree = NULL;
    bool selected = false;

    resrc_tree = resrc_tree_list_first (found_children);
    while (resrc_tree) {
        selected = false;
        if (resrc_match_resource (resrc_tree_resrc (resrc_tree), child_reqst,
                                  true)) {
            if (resrc_reqst_num_children (child_reqst)) {
                if (resrc_tree_num_children (resrc_tree)) {
                    child_tree = resrc_tree_new (parent_tree,
                                                 resrc_tree_resrc (resrc_tree));
                    if (select_children (h, resrc_tree_children (resrc_tree),
                                         resrc_reqst_children (child_reqst),
                                         child_tree)) {
                        resrc_reqst_add_found (child_reqst, 1);
                        resrc_stage_resrc (resrc_tree_resrc (resrc_tree),
                                           resrc_reqst_reqrd_size (child_reqst));
                        selected = true;
                        if (resrc_reqst_nfound (child_reqst) >=
                            resrc_reqst_reqrd_qty (child_reqst))
                            goto ret;
                    } else {
                        resrc_tree_destroy (child_tree, false);
                    }
                }
            } else {
                (void) resrc_tree_new (parent_tree,
                                       resrc_tree_resrc (resrc_tree));
                resrc_reqst_add_found (child_reqst, 1);
                resrc_stage_resrc (resrc_tree_resrc (resrc_tree),
                                   resrc_reqst_reqrd_size (child_reqst));
                selected = true;
                if (resrc_reqst_nfound (child_reqst) >=
                    resrc_reqst_reqrd_qty (child_reqst))
                    goto ret;
            }
        }
        /*
         * The following clause allows the resource request to be
         * sparsely defined.  E.g., it might only stipulate a node
         * with 4 cores and omit the intervening socket.
         */
        if (!selected) {
            if (resrc_tree_num_children (resrc_tree)) {
                child_tree = resrc_tree_new (parent_tree,
                                             resrc_tree_resrc (resrc_tree));
                if (select_child (h, resrc_tree_children (resrc_tree),
                                  child_reqst, child_tree)) {
                    selected = true;
                    if (resrc_reqst_nfound (child_reqst) >=
                        resrc_reqst_reqrd_qty (child_reqst))
                        goto ret;
                } else {
                    resrc_tree_destroy (child_tree, false);
                }
            }
        }
        resrc_tree = resrc_tree_list_next (found_children);
    }
ret:
    return selected;
}


static bool select_children (flux_t h, resrc_tree_list_t *found_children,
                             resrc_reqst_list_t *reqst_children,
                             resrc_tree_t *parent_tree)
{
    resrc_reqst_t *child_reqst = resrc_reqst_list_first (reqst_children);
    bool selected = false;

    while (child_reqst) {
        resrc_reqst_clear_found (child_reqst);
        selected = false;

        if (select_child (h, found_children, child_reqst, parent_tree) &&
            (resrc_reqst_nfound (child_reqst) >=
             resrc_reqst_reqrd_qty (child_reqst)))
            selected = true;

        if (!selected)
            break;

        child_reqst = resrc_reqst_list_next (reqst_children);
    }

    return selected;
}

/*
 * select_resources() selects from the set of resource candidates the
 * best resources for the job.  If reserve is set, whatever resources
 * are selected will be reserved for the job and removed from
 * consideration as candidates for other jobs.
 *
 * Inputs:  found_trees - list of resource tree candidates
 *          resrc_reqst - the resources the job requests
 * Returns: a list of selected resource trees, or null if none (or not enough)
 *          are selected
 */
resrc_tree_list_t *select_resources (flux_t h, resrc_tree_list_t *found_trees,
                                     resrc_reqst_t *resrc_reqst)
{
    int64_t reqrd_qty;
    resrc_t *resrc;
    resrc_tree_list_t *selected_res = NULL;
    resrc_tree_t *new_tree = NULL;
    resrc_tree_t *rt;

    if (!resrc_reqst) {
        flux_log (h, LOG_ERR, "%s: called with empty request", __FUNCTION__);
        return NULL;
    }

    reqrd_qty = resrc_reqst_reqrd_qty (resrc_reqst);
    /*
     * A start time of zero means that we are only interested in
     * if the resource is available now.
     */
    resrc_reqst_set_starttime (resrc_reqst, 0);
    selected_res = resrc_tree_list_new ();

    rt = resrc_tree_list_first (found_trees);
    while (reqrd_qty && rt) {
        resrc = resrc_tree_resrc (rt);
        if (resrc_match_resource (resrc, resrc_reqst, true)) {
            new_tree = resrc_tree_new (NULL, resrc);
            if (resrc_reqst_num_children (resrc_reqst)) {
                if (resrc_tree_num_children (rt)) {
                    if (select_children (h, resrc_tree_children (rt),
                                         resrc_reqst_children (resrc_reqst),
                                         new_tree)) {
                        resrc_tree_list_append (selected_res, new_tree);
                        resrc_stage_resrc (resrc,
                                           resrc_reqst_reqrd_size (resrc_reqst));
                        flux_log (h, LOG_DEBUG, "selected %s", resrc_name (resrc));
                        reqrd_qty--;
                    } else {
                        resrc_tree_destroy (new_tree, false);
                    }
                }
            } else {
                resrc_tree_list_append (selected_res, new_tree);
                resrc_stage_resrc (resrc, resrc_reqst_reqrd_size (resrc_reqst));
                flux_log (h, LOG_DEBUG, "selected %s", resrc_name (resrc));
                reqrd_qty--;
            }
        }
        rt = resrc_tree_list_next (found_trees);
    }

    /* If we did not select all that was required and the selected
     * resource list is empty, destroy the list. */
    if (reqrd_qty && !resrc_tree_list_size (selected_res)) {
        resrc_tree_list_destroy (selected_res, false);
        selected_res = NULL;
    }

    return selected_res;
}

int allocate_resources (flux_t h, resrc_tree_list_t *rtl, int64_t job_id,
                        int64_t starttime, int64_t endtime)
{
    int rc = resrc_tree_list_allocate (rtl, job_id, 0, 0);
    return rc;
}

int reserve_resources (flux_t h, resrc_tree_list_t *rtl, int64_t job_id,
                       int64_t starttime, int64_t walltime, resrc_t *resrc,
                       resrc_reqst_t *resrc_reqst)
{
    int rc = -1;

    if (rtl && !(rc = resrc_tree_list_reserve (rtl, job_id, 0, 0)))
        flux_log (h, LOG_DEBUG, "Reserved %"PRId64" nodes for job %"PRId64"",
                  resrc_tree_list_size (rtl), job_id);
    return rc;
}


MOD_NAME ("sched.plugin1");


/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
