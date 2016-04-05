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
#include <dlfcn.h>
#include <time.h>
#include <inttypes.h>

#include "src/common/libutil/log.h"
#include "src/common/libutil/shortjson.h"
#include "src/common/libutil/xzmalloc.h"
#include "rdl.h"
#include "scheduler.h"
#include "simulator.h"

static flux_t h = NULL;
static ctx_t *ctx = NULL;

bool allocate_bandwidth (flux_lwj_t *job,
                         struct resource *r,
                         zlist_t *ancestors)
{
    int64_t avail_bw;
    struct resource *curr_r = NULL;
    // Check if the resource has enough bandwidth
    avail_bw = get_avail_bandwidth (r);

    if (avail_bw < job->req.io_rate) {
        // JSON o = rdl_resource_json (r);
        // flux_log (h, LOG_DEBUG, "not enough bandwidth (has: %ld, needs: %ld)
        // at %s", avail_bw, job->req.io_rate, Jtostr (o));
        // Jput (o);
        return false;
    }

    // Check if the ancestors have enough bandwidth
    curr_r = zlist_first (ancestors);
    while (curr_r != NULL) {
        avail_bw = get_avail_bandwidth (curr_r);
        if (avail_bw < job->req.io_rate) {
            // JSON o = rdl_resource_json (curr_r);
            // flux_log (h, LOG_DEBUG, "not enough bandwidth (has: %ld, needs:
            // %ld) at %s", avail_bw, job->req.io_rate, Jtostr (o));
            // Jput (o);
            return false;
        }
        curr_r = zlist_next (ancestors);
    }

    // If not, return false, else allocate the bandwith
    // at resource and ancestors then return true
    allocate_resource_bandwidth (r, job->req.io_rate);
    curr_r = zlist_first (ancestors);
    while (curr_r != NULL) {
        allocate_resource_bandwidth (curr_r, job->req.io_rate);
        curr_r = zlist_next (ancestors);
    }

    return true;
}

int schedule_jobs (ctx_t *ctx, double sim_time)
{
    struct rdl *rdl = ctx->rdl, *free_rdl = NULL;
    char *uri = ctx->uri;
    zlist_t *jobs = ctx->p_queue;
    flux_lwj_t *job = NULL;
    int rc = 0, job_scheduled = 1;
    int64_t curr_free_cores = -1;

    free_rdl = get_free_subset (rdl, "core");
    if (free_rdl) {
        curr_free_cores = get_free_count (free_rdl, uri, "core");
    } else {
        flux_log (h,
                  LOG_DEBUG,
                  "get_free_subset returned nothing, setting curr_free_cores = "
                  "0");
        curr_free_cores = 0;
    }

    zlist_sort (jobs, job_compare_t);
    job = zlist_first (jobs);
    while (job_scheduled && job) {
        if (job->state == j_unsched) {
            job_scheduled =
                schedule_job (ctx, rdl, free_rdl, uri, curr_free_cores, job);
            if (job_scheduled) {
                curr_free_cores -= job->alloc.ncores;
                remove_job_resources_from_rdl (free_rdl, uri, job);
                rc += 1;
            }
        }
        job = zlist_next (jobs);
    }
    flux_log (h, LOG_DEBUG, "Finished iterating over the jobs list");

    rdl_destroy (free_rdl);
    return rc;
}

/****************************************************************
 *
 *        High Level Job and Resource Event Handlers
 *
 ****************************************************************/
static struct flux_msg_handler_spec htab[] = {
    {FLUX_MSGTYPE_EVENT, "sim.start", start_cb},
    {FLUX_MSGTYPE_REQUEST, "sim_sched.trigger", trigger_cb},
    //{ FLUX_MSGTYPE_EVENT,   "sim_sched.event",   event_cb },
    //{FLUX_MSGTYPE_REQUEST, "sim_sched.lwj-watch", newlwj_rpc},
    FLUX_MSGHANDLER_TABLE_END,
};

int mod_main (flux_t p, int argc, char **argv)
{
    zhash_t *args = zhash_fromargv (argc, argv);
    h = p;
    ctx = getctx (h);

    return init_and_start_scheduler (h, ctx, args, htab);
}

MOD_NAME ("sim_sched");

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
