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

/*
 * schedplugin1.c - backfill sheduling services
 *
 * Update Log:
 *       Aug 7 2014 DAL: File created.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <czmq.h>
#include <json.h>
#include <flux/core.h>

#include "src/common/libutil/jsonutil.h"
#include "src/common/libutil/log.h"
#include "src/common/libutil/shortjson.h"
#include "src/common/libutil/xzmalloc.h"
#include "resrc.h"
#include "resrc_tree.h"
#include "schedsrv.h"

//TODO: this plugin inherently must know the inner structure of the opaque
//types, something we need to think about
typedef struct zhash_t resources;


/*
 * find_resources() identifies the all of the resource candidates for
 * the job.  The set of resources returned could be more than the job
 * requires.  A later call to select_resources() will cull this list
 * down to the most appropriate set for the job.  If less resources
 * are found than the job requires, and if the job asks to reserve
 * resources, then *preserve will be set to true.
 *
 * Inputs:  resrcs - hash table of all resources
 *          job - the job requesting the resources
 * Returns: a list of resource keys satisfying the job's request,
 *                     or NULL if none (or not enough) are found
 *          preserve - value set to true if not enough resources were found
 *                     and the job wants these resources reserved
 */
resrc_tree_list_t *find_resources (flux_t h, resources_t *resrcs,
                                   flux_lwj_t *job, bool *preserve)
{
    JSON child_core = NULL;
    JSON req_res = NULL;
    int64_t found = 0;
    resrc_reqst_t *resrc_reqst = NULL;
    resrc_t *resrc = NULL;
    resrc_tree_list_t *found_trees = NULL;
    resrc_tree_t *resrc_tree = NULL;

    if (!resrcs || !job) {
        flux_log (h, LOG_ERR, "find_resources invalid arguments");
        goto ret;
    }

    resrc = zhash_lookup ((zhash_t *)resrcs, "head");
    if (resrc) {
        resrc_tree = resrc_phys_tree (resrc);
    } else {
        printf ("Failed to find head resource\n");
        goto ret;
    }

    found_trees = resrc_tree_new_list ();
    if (!found_trees) {
        flux_log (h, LOG_ERR, "find_resources new tree list failed");
        goto ret;
    }

    /*
     * Require at least one task per node, and
     * Assume (for now) one task per core.
     */
    if (job->req->ncores < job->req->nnodes)
        job->req->ncores = job->req->nnodes;
    job->req->corespernode = (job->req->ncores + job->req->nnodes - 1) /
        job->req->nnodes;

    child_core = Jnew ();
    Jadd_str (child_core, "type", "core");
    Jadd_int (child_core, "req_qty", job->req->corespernode);

    req_res = Jnew ();
    Jadd_str (req_res, "type", "node");
    Jadd_int (req_res, "req_qty", job->req->nnodes);
    json_object_object_add (req_res, "req_child", child_core);

    if ((resrc_reqst = resrc_reqst_from_json (req_res, NULL))) {
        found = resrc_tree_search (resrc_tree_children (resrc_tree),
                                   resrc_reqst, found_trees, true);
        resrc_reqst_destroy (resrc_reqst);
    }
    Jput (req_res);

    if (found >= job->req->nnodes) {
        flux_log (h, LOG_DEBUG, "%ld composites found for lwj.%ld req: %ld",
                  found, job->lwj_id, job->req->nnodes);
    } else if (found && job->reserve) {
        *preserve = true;
        flux_log (h, LOG_DEBUG, "%ld composites reserved for lwj.%ld's req %ld",
                  found, job->lwj_id, job->req->nnodes);
    }

    if (!resrc_tree_list_size (found_trees)) {
        resrc_tree_list_destroy (found_trees);
        found_trees = NULL;
    }
ret:
    return found_trees;
}

static uint64_t select_children (flux_t h, resrc_tree_list_t *children,
                                 resrc_tree_list_t *selected_res, char *type,
                                 uint64_t *pcount)
{
    resrc_tree_t *child = NULL;
    uint64_t selected = 0;

    child = resrc_tree_list_first (children);
    while (child && *pcount) {
        if (!strcmp (resrc_type (resrc_tree_resrc (child)), type)) {
            flux_log (h, LOG_DEBUG, "selected %s: %s", type,
                      resrc_type (resrc_tree_resrc (child)));
            (*pcount)--;
            selected++;
        } else if (resrc_tree_list_size (resrc_tree_children (child))) {
            selected += select_children (h, resrc_tree_children (child),
                                         selected_res, type, pcount);
        }
        child = resrc_tree_list_next (children);
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
 *          job - the job for which we are selecting the resources
 *          reserve - if true and if not enough resources were found,
 *                    return what was found, to be reserved for the
 *                    job at a subsequent step.
 * Returns: a list of selected resource trees, or null if none (or not enough)
 *          are selected
 */
resrc_tree_list_t *select_resources (flux_t h, resrc_tree_list_t *found_trees,
                                     flux_lwj_t *job, bool reserve)
{
    resrc_t *resrc;
    resrc_tree_t *rt;
    uint64_t cpn;       /* cores per node */
    uint64_t ctn;       /* cores this node */
    uint64_t ncores;
    uint64_t nnodes;
    resrc_tree_list_t *selected_res = NULL;

    if (!job) {
        flux_log (h, LOG_ERR, "select_resources called with null job");
        return NULL;
    }

    ncores = job->req->ncores;
    nnodes = job->req->nnodes;
    cpn = job->req->corespernode;
    selected_res = resrc_tree_new_list ();

    rt = resrc_tree_list_first (found_trees);
    while (nnodes && rt) {
        resrc = resrc_tree_resrc (rt);
        if (!strncmp (resrc_type (resrc), "node", 5)) {
            ctn = MIN (ncores, cpn);

            if (ctn && resrc_tree_list_size (resrc_tree_children (resrc_phys_tree
                                                                  (resrc)))) {
                ncores -= select_children (h, resrc_tree_children
                                           (resrc_phys_tree (resrc)),
                                           selected_res, "core", &ctn);
            }
            zlist_append ((zlist_t *) selected_res, rt);
            flux_log (h, LOG_DEBUG, "selected node: %s", resrc_name (resrc));
            nnodes--;
            rt = resrc_tree_list_next (found_trees);
        }
    }

    if ((nnodes || ncores) && !reserve) {
        zlist_destroy ((zlist_t **) &selected_res);
        selected_res = NULL;
    }

    return (resrc_tree_list_t*)selected_res;
}

MOD_NAME ("sched.plugin1");


/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
