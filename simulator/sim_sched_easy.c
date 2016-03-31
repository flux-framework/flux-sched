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
    struct resource *curr_r = NULL;

    allocate_resource_bandwidth (r, job->req.io_rate);
    curr_r = zlist_first (ancestors);
    while (curr_r != NULL) {
        allocate_resource_bandwidth (curr_r, job->req.io_rate);
        curr_r = zlist_next (ancestors);
    }

    return true;
}

// Calculate the earliest point in time where the number of free cores
// is greater than the number of cores required by the reserved job.
// Output is the time at which this occurs and the number of cores that
// are free, excluding the cores that will be used by the reserved job
void calculate_shadow_info (flux_lwj_t *reserved_job,
                            struct rdl *rdl,
                            const char *uri,
                            zlist_t *running_jobs,
                            double *shadow_time,
                            int64_t *shadow_free_cores)
{
    job_t *curr_job_t;
    struct rdl *frdl = NULL;

    if (zlist_size (running_jobs) == 0) {
        flux_log (h,
                  LOG_ERR,
                  "No running jobs and reserved job still doesn't fit.");
        return;
    } else {
        flux_log (h,
                  LOG_DEBUG,
                  "calculate_shadow_info found %zu jobs currently running",
                  zlist_size (running_jobs));
    }

    frdl = get_free_subset (rdl, "core");
    if (frdl) {
        *shadow_free_cores = get_free_count (frdl, uri, "core");
        rdl_destroy (frdl);
    } else {
        flux_log (h,
                  LOG_DEBUG,
                  "get_free_subset returned nothing, setting shadow_free_cores "
                  "= 0");
        *shadow_free_cores = 0;
    }

    zlist_sort (running_jobs, job_compare_termination_fn);

    curr_job_t = zlist_first (running_jobs);
    while ((*shadow_free_cores < reserved_job->req.ncores) && curr_job_t) {
        flux_log (h,
                  LOG_DEBUG,
                  "reserved_job_req_cores: %d, free cores: %ld, curr_job: %d, "
                  "curr_job_ncores: %d",
                  reserved_job->req.ncores,
                  *shadow_free_cores,
                  curr_job_t->id,
                  curr_job_t->ncpus);
        *shadow_free_cores += curr_job_t->ncpus;
        *shadow_time = curr_job_t->start_time + curr_job_t->time_limit;
        curr_job_t = zlist_next (running_jobs);
    }

    // Subtract out the cores that will be used by reserved job
    *shadow_free_cores -= reserved_job->req.ncores;
}

// Determines if a job is eligible for backfilling or not
// Written as a series of if statements for clarity
bool job_backfill_eligible (flux_lwj_t *job,
                            int64_t curr_free_cores,
                            int64_t shadow_free_cores,
                            double shadow_time,
                            double sim_time)
{
    bool terminates_before_shadow_time =
        ((sim_time + job->req.walltime) < shadow_time);
    int64_t min_of_curr_shadow_cores = (curr_free_cores < shadow_free_cores)
                                           ? curr_free_cores
                                           : shadow_free_cores;

    flux_log (h,
              LOG_DEBUG,
              "backfill info - shadow_time: %f, curr_free_cores: %ld, "
              "shadow_free_cores: %ld, job_req_cores: %d",
              shadow_time,
              curr_free_cores,
              shadow_free_cores,
              job->req.ncores);
    flux_log (h,
              LOG_DEBUG,
              "backfill info - term_before_shadow_time: %d, "
              "min_of_curr_shadow_cores: %ld",
              terminates_before_shadow_time,
              min_of_curr_shadow_cores);

    if (terminates_before_shadow_time && curr_free_cores >= job->req.ncores) {
        // Job ends before reserved job starts, make sure we have enough cores
        // currently to schedule it
        return true;
    } else if (!terminates_before_shadow_time
               && min_of_curr_shadow_cores >= job->req.ncores) {
        // Job ends after reserved job starts, make sure we will have
        // enough free cores at shadow time to not delay the reserved
        // job
        return true;
    } else {
        return false;
    }
}

int schedule_jobs (ctx_t *ctx, double sim_time)
{
    flux_lwj_t *curr_queued_job = NULL, *reserved_job = NULL,
               *curr_running_job = NULL;
    job_t *curr_job_t;
    kvsdir_t *curr_kvs_dir = NULL;
    int rc = 0, job_scheduled = 1;
    double shadow_time = -1;
    int64_t shadow_free_cores = -1, curr_free_cores = -1;
    struct rdl *rdl = ctx->rdl, *free_rdl = NULL;
    char *uri = ctx->uri;
    zlist_t *jobs = ctx->p_queue, *queued_jobs = zlist_dup (jobs),
            *running_jobs = zlist_new ();

    zlist_sort (queued_jobs, job_compare_t);
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

    // Schedule all jobs at the front of the queue
    curr_queued_job = zlist_first (queued_jobs);
    while (job_scheduled && curr_queued_job) {
        if (curr_queued_job->state == j_unsched) {
            job_scheduled = schedule_job (
                ctx, rdl, free_rdl, uri, curr_free_cores, curr_queued_job);
            if (job_scheduled) {
                curr_free_cores -= curr_queued_job->alloc.ncores;
                rc += 1;
                if (kvs_get_dir (
                        h, &curr_kvs_dir, "lwj.%ld", curr_queued_job->lwj_id)) {
                    flux_log (h,
                              LOG_ERR,
                              "lwj.%ld kvsdir not found",
                              curr_queued_job->lwj_id);
                } else {
                    curr_job_t = pull_job_from_kvs (curr_kvs_dir);
                    if (curr_job_t->start_time == 0)
                        curr_job_t->start_time = sim_time;
                    zlist_append (running_jobs, curr_job_t);
                }
            } else {
                reserved_job = curr_queued_job;
            }
        }
        curr_queued_job = zlist_next (queued_jobs);
    }

    // reserved job is now set, start backfilling
    if (reserved_job && curr_queued_job) {
        curr_running_job = zlist_first (ctx->r_queue);
        while (curr_running_job != NULL) {
            if (kvs_get_dir (
                    h, &curr_kvs_dir, "lwj.%ld", curr_running_job->lwj_id)) {
                flux_log (h,
                          LOG_ERR,
                          "lwj.%ld kvsdir not found",
                          curr_running_job->lwj_id);
            } else {
                curr_job_t = pull_job_from_kvs (curr_kvs_dir);
                if (curr_job_t->start_time == 0)
                    curr_job_t->start_time = sim_time;
                zlist_append (running_jobs, curr_job_t);
            }
            curr_running_job = zlist_next (ctx->r_queue);
        }
        calculate_shadow_info (reserved_job,
                               rdl,
                               uri,
                               running_jobs,
                               &shadow_time,
                               &shadow_free_cores);
        flux_log (h,
                  LOG_DEBUG,
                  "Job %ld has the reservation",
                  reserved_job->lwj_id);
        flux_log (h,
                  LOG_DEBUG,
                  "Shadow info - shadow time: %f, shadow free cores: %ld",
                  shadow_time,
                  shadow_free_cores);

        while (curr_queued_job && shadow_time >= 0 && curr_free_cores > 0) {
            if (curr_queued_job->state == j_unsched
                && job_backfill_eligible (curr_queued_job,
                                          curr_free_cores,
                                          shadow_free_cores,
                                          shadow_time,
                                          sim_time)) {
                flux_log (h,
                          LOG_DEBUG,
                          "Job %ld is eligible for backfilling. Attempting to "
                          "schedule.",
                          curr_queued_job->lwj_id);

                job_scheduled = schedule_job (
                    ctx, rdl, free_rdl, uri, curr_free_cores, curr_queued_job);
                if (job_scheduled) {
                    curr_free_cores -= curr_queued_job->alloc.ncores;
                    rc += 1;
                    if ((sim_time + curr_queued_job->req.walltime)
                        >= shadow_time) {
                        shadow_free_cores -= curr_queued_job->alloc.ncores;
                    }
                    flux_log (h,
                              LOG_DEBUG,
                              "Job %ld was backfilled.",
                              curr_queued_job->lwj_id);
                } else {
                    flux_log (h,
                              LOG_DEBUG,
                              "Job %ld was not backfillied",
                              curr_queued_job->lwj_id);
                }
            } else {
                flux_log (h,
                          LOG_DEBUG,
                          "Job %ld is not eligible for backfilling, moving on "
                          "to the next job.",
                          curr_queued_job->lwj_id);
            }
            curr_queued_job = zlist_next (queued_jobs);
        }
    }

    flux_log (h, LOG_DEBUG, "Finished iterating over the queued_jobs list");
    // Cleanup
    curr_job_t = zlist_pop (running_jobs);
    while (curr_job_t != NULL) {
        free_job (curr_job_t);
        curr_job_t = zlist_pop (running_jobs);
    }
    zlist_destroy (&running_jobs);
    zlist_destroy (&queued_jobs);
    return rc;
}

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
