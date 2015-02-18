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
typedef struct zlist_t resource_list;

static const char* CORETYPE = "core";
static const char* NODETYPE = "node";

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
resource_list_t *find_resources (flux_t h, resources_t *resrcs, flux_lwj_t *job,
                         bool *preserve)
{
    JSON child_core = NULL;
    JSON req_res = NULL;
    int64_t found = 0;
    resource_list_t *found_res = NULL;              /* found resources */
    resrc_t *resrc = NULL;
    resrc_tree_t *resrc_tree = NULL;

    if (!resrcs || !job) {
        flux_log (h, LOG_ERR, "find_resources invalid arguments");
        goto ret;
    }

    resrc = zhash_lookup ((zhash_t *)resrcs, "cluster.0");
    if (resrc) {
        resrc_tree = resrc_phys_tree (resrc);
    } else {
        printf ("Failed to find cluster.0 resource\n");
        goto ret;
    }

    found_res = resrc_new_id_list ();
    if (!found_res) {
        flux_log (h, LOG_ERR, "find_resources new list failed");
        goto ret;
    }

    child_core = Jnew ();
    Jadd_str (child_core, "type", "core");
    Jadd_int (child_core, "req_qty", job->req->ncores);

    req_res = Jnew ();
    Jadd_str (req_res, "type", "node");
    Jadd_int (req_res, "req_qty", job->req->nnodes);
    json_object_object_add (req_res, "req_child", child_core);

    found = resrc_tree_search (resrc_tree_children (resrc_tree), found_res,
                               req_res, true);
    if (found >= job->req->nnodes) {
        flux_log (h, LOG_DEBUG, "%ld composites found for lwj.%ld req: %ld",
                  found, job->lwj_id, job->req->nnodes);
    } else if (found && job->reserve) {
        *preserve = true;
        flux_log (h, LOG_DEBUG, "%ld composites reserved for lwj.%ld's req %ld",
                  found, job->lwj_id, job->req->nnodes);
    }

    if (!resrc_list_size (found_res)) {
        resrc_id_list_destroy (found_res);
        found_res = NULL;
    }
ret:
    return found_res;
}

/*
 * select_resources() selects from the set of resource candidates the
 * best resources for the job.  If reserve is set, whatever resources
 * are selected will be reserved for the job and removed from
 * consideration as candidates for other jobs.
 *
 * Inputs:  resrcs - hash table of all resources
 *          resrc_ids - list of keys to resource candidates
 *          job - the job for which we are selecting the resources
 *          reserve - if true, we reserve selected resources, thereby removing
 *                    them from being considered for other jobs
 * Returns: a list of selected resource keys, or null if none (or not enough)
 *          are selected
 */
resource_list_t *select_resources (flux_t h, resources_t *resrcs_in,
                                   resource_list_t *resrc_ids_in,
                                   flux_lwj_t *job, bool reserve)
{
    zlist_t * resrc_ids = (zlist_t*)resrc_ids_in;
    zhash_t * resrcs = (zhash_t*)resrcs_in;
    char *resrc_id;
    char *new_id;
    resrc_t *resrc;
    uint64_t ncores;
    uint64_t nnodes;
    zlist_t *selected_res = NULL;

    if (!job) {
        flux_log (h, LOG_ERR, "select_resources called with null job");
        return NULL;
    }

    ncores = job->req->ncores;
    nnodes = job->req->nnodes;
    selected_res = zlist_new ();

    resrc_id  = zlist_first (resrc_ids);
    while (resrc_id) {
        resrc = zhash_lookup (resrcs, resrc_id);
        if (nnodes && !strncmp (resrc_type(resrc), NODETYPE,
                                sizeof (NODETYPE))) {
            nnodes--;
            new_id = xstrdup (resrc_id);
            zlist_append (selected_res, new_id);
            flux_log (h, LOG_DEBUG, "selected node: %s", resrc_id);
        } else if (ncores && !strncmp (resrc_type(resrc), CORETYPE,
                                       sizeof (CORETYPE))) {
            ncores--;
            new_id = xstrdup (resrc_id);
            zlist_append (selected_res, new_id);
            flux_log (h, LOG_DEBUG, "selected core: %s", resrc_id);
        }
        resrc_id = zlist_next (resrc_ids);
    }

    if ((nnodes || ncores) && !reserve) {
        zlist_destroy (&selected_res);
        selected_res = NULL;
    }

    return (resource_list_t*)selected_res;
}

MOD_NAME ("sched.plugin1");


/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
