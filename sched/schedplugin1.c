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
 * down to the most appropriate set for the job.
 *
 * Inputs:  resrcs - hash table of all resources
 *          resrc_reqst - the resources the job requests
 * Returns: a list of resource trees satisfying the job's request,
 *                   or NULL if none (or not enough) are found
 */
resrc_tree_list_t *find_resources (flux_t h, resources_t *resrcs,
                                   resrc_reqst_t *resrc_reqst, bool *preserve)
{
    int64_t nfound = 0;
    resrc_t *resrc = NULL;
    resrc_tree_list_t *found_trees = NULL;
    resrc_tree_t *resrc_tree = NULL;

    if (!resrcs || !resrc_reqst) {
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

    nfound = resrc_tree_search (resrc_tree_children (resrc_tree), resrc_reqst,
                               found_trees, true);

    if (!nfound) {
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
