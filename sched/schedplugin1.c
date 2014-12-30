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
#include "schedsrv.h"

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
zlist_t *find_resources (flux_t h, zhash_t *resrcs, flux_lwj_t *job,
                         bool *preserve)
{
    int64_t found = 0;
    zlist_t *found_res = NULL;              /* found resources */

    if (!resrcs || !job) {
        flux_log (h, LOG_ERR, "find_resources invalid arguments");
        goto ret;
    }

    found_res = zlist_new ();
    if (!found_res) {
        flux_log (h, LOG_ERR, "find_resources new list failed");
        goto ret;
    }

    found = resrc_find_resources (resrcs, found_res, NODETYPE, true);
    if (found >= job->req->nnodes) {
        flux_log (h, LOG_DEBUG, "%ld nodes found for lwj.%ld req: %ld",
                  found, job->lwj_id, job->req->nnodes);
    } else if (found && job->reserve) {
        *preserve = true;
        flux_log (h, LOG_DEBUG, "%ld nodes reserved for lwj.%ld's req %ld",
                  found, job->lwj_id, job->req->nnodes);
    }

    found = resrc_find_resources (resrcs, found_res, CORETYPE, true);
    if (found >= job->req->ncores) {
        flux_log (h, LOG_DEBUG, "%ld cores found for lwj.%ld req: %ld",
                  found, job->lwj_id, job->req->ncores);
    } else if (found && job->reserve) {
        *preserve = true;
        flux_log (h, LOG_DEBUG, "%ld cores reserved for lwj.%ld's req %ld",
                  found, job->lwj_id, job->req->ncores);
    }

    if (!zlist_size (found_res)) {
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
zlist_t *select_resources (flux_t h, zhash_t *resrcs, zlist_t *resrc_ids,
                           flux_lwj_t *job, bool reserve)
{
    char *resrc_id;
    char *new_id;
    resource_t *resrc;
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
        resrc_id_list_destroy (selected_res);
        selected_res = NULL;
    }

    return selected_res;
}

MOD_NAME ("sched.plugin1");


/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
