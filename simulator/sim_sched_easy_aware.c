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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <czmq.h>
#include <json.h>
#include <dlfcn.h>
#include <time.h>
#include <inttypes.h>

#include "src/common/libutil/jsonutil.h"
#include "src/common/libutil/log.h"
#include "src/common/libutil/shortjson.h"
#include "src/common/libutil/xzmalloc.h"
#include "rdl.h"
#include "scheduler.h"
#include "simulator.h"

static flux_t h = NULL;
static ctx_t *ctx = NULL;

bool allocate_bandwidth (flux_lwj_t *job, struct resource *r, zlist_t *ancestors)
{
	int64_t avail_bw;
	struct resource *curr_r = NULL;
	//Check if the resource has enough bandwidth
	avail_bw = get_avail_bandwidth (r);

    flux_log (h, LOG_DEBUG, "req_io: %ld, avail_bw: %ld", job->req.io_rate, avail_bw);

	if (avail_bw < job->req.io_rate) {
        return false;
	}

	//Check if the ancestors have enough bandwidth
 	curr_r = zlist_first (ancestors);
	while (curr_r != NULL) {
		avail_bw = get_avail_bandwidth (curr_r);
		if (avail_bw < job->req.io_rate) {
			return false;
		}
		curr_r = zlist_next (ancestors);
	}

	//If not, return false, else allocate the bandwith
	//at resource and ancestors then return true
	allocate_resource_bandwidth (r, job->req.io_rate);
 	curr_r = zlist_first (ancestors);
	while (curr_r != NULL) {
		allocate_resource_bandwidth (curr_r, job->req.io_rate);
		curr_r = zlist_next (ancestors);
	}

	return true;
}

static int schedule_job_without_update (struct rdl *rdl, struct rdl *free_rdl, const char *uri,
                                        int64_t free_cores, flux_lwj_t *job, struct rdl_accumulator **a)
{
    //int64_t nodes = -1;
    int rc = 0;

    struct resource *free_root = NULL;         /* found resource */

    if (!job || !rdl || !uri) {
        flux_log (h, LOG_ERR, "schedule_job invalid arguments");
        goto ret;
    }
    flux_log (h, LOG_DEBUG, "schedule_job_without_update called on job %ld", job->lwj_id);

    free_root = rdl_resource_get (free_rdl, uri);

	if (free_rdl && free_root && free_cores >= job->req.ncores) {
        zlist_t *ancestors = zlist_new ();
        //TODO: revert this in the deallocation/rollback
        int old_nnodes = job->req.nnodes;
        int old_ncores = job->req.ncores;
        int old_io_rate = job->req.io_rate;
        int old_alloc_nnodes = job->alloc.nnodes;
        int old_alloc_ncores = job->alloc.ncores;

        rdl_resource_iterator_reset (free_root);
        *a = rdl_accumulator_create (rdl);
        if (allocate_resources (rdl, uri, free_root, *a, job, ancestors)) {
            flux_log (h, LOG_INFO, "scheduled job %ld without update", job->lwj_id);
            if (rc == 0)
                rc = 1;
        } else {
            if (rdl_accumulator_is_empty(*a)) {
                flux_log (h, LOG_DEBUG, "no resources found in accumulator");
            } else {
                job->rdl = rdl_accumulator_copy (*a);
                release_resources (ctx, rdl, uri, job);
                rdl_destroy (job->rdl);
                rdl_accumulator_destroy (*a);
            }
        }
        job->req.io_rate = old_io_rate;
        job->req.nnodes = old_nnodes;
        job->req.ncores = old_ncores;
        job->alloc.nnodes = old_alloc_nnodes;
        job->alloc.ncores = old_alloc_ncores;

        //TODO: clear the list and free each element (or set freefn)
        zlist_destroy (&ancestors);
    } else {
        flux_log (h, LOG_DEBUG, "not enough available cores, skipping this job");
    }
    rdl_resource_destroy (free_root);

ret:
    return rc;
}


//Calculate the earliest point in time where the number of free cores
//is greater than the number of cores required by the reserved job.
//Output is the time at which this occurs and the number of cores that
//are free, excluding the cores that will be used by the reserved job

//The input rdl should be a copy that can be mutated to reflect the rdl's state @ shadow_time
static void calculate_shadow_info (flux_lwj_t *reserved_job, struct rdl *rdl, const char *uri,
                                   zlist_t *running_jobs,
                                   //output
                                   struct rdl **shadow_rdl, double *shadow_time)
{
    job_t *curr_job_t = NULL;
    flux_lwj_t *curr_lwj_job = NULL;
    struct rdl *shadow_free_rdl = NULL;
    int64_t shadow_free_cores = -1;

    if (zlist_size (running_jobs) == 0) {
        flux_log (h, LOG_ERR, "No running jobs and reserved job still doesn't fit.");
        *shadow_rdl = NULL;
        *shadow_time = -1;
        return;
    } else {
        flux_log (h, LOG_DEBUG, "calculate_shadow_info found %zu jobs currently running", zlist_size (running_jobs));
    }

    *shadow_rdl = rdl_copy (rdl);
    shadow_free_rdl = get_free_subset (*shadow_rdl, "core");
    if (shadow_free_rdl) {
        shadow_free_cores = get_free_count (shadow_free_rdl, uri, "core");
    } else {
        flux_log (h, LOG_DEBUG, "get_free_subset returned nothing, setting shadow_free_cores = 0");
        shadow_free_cores = 0;
    }

    zlist_sort(running_jobs, job_compare_termination_fn);

    curr_job_t = zlist_first (running_jobs);
    while (shadow_free_cores < reserved_job->req.ncores) {
        if (curr_job_t == NULL) {
            flux_log (h, LOG_ERR, "Curr job is null");
            break;
        } else if (curr_job_t->ncpus < 1) {
            flux_log (h, LOG_ERR, "Curr job %d incorrectly requires < 1 cpu: %d", curr_job_t->id, curr_job_t->ncpus);
        }

        //De-allocate curr_job_t's resources from the *shadow_rdl
        curr_lwj_job = find_lwj(ctx, curr_job_t->id);
        if (curr_lwj_job->alloc.ncores != curr_job_t->ncpus) {
            flux_log (h, LOG_ERR, "Job %d's ncpus don't match: %d (lwj_job) and %d (job_t)", curr_job_t->id, curr_lwj_job->alloc.ncores, curr_job_t->ncpus);
        }
        release_resources (ctx, *shadow_rdl, uri, curr_lwj_job);

        shadow_free_cores += curr_job_t->ncpus;
        *shadow_time = curr_job_t->start_time + curr_job_t->time_limit;
        curr_job_t = zlist_next (running_jobs);
    }

    flux_log (h, LOG_DEBUG, "Entering the exact shadow loop");

    rdl_destroy (shadow_free_rdl);
    shadow_free_rdl = get_free_subset (*shadow_rdl, "core");

    //Do a loop checking if the reserved job can be scheduled (considering IO)
    struct rdl_accumulator *accum = NULL;
    while (!schedule_job_without_update (*shadow_rdl, shadow_free_rdl, uri,
                                         shadow_free_cores, reserved_job, &accum)) {
        if (curr_job_t == NULL) {
            flux_log (h, LOG_ERR, "Curr job is null");
            break;
        } else if (curr_job_t->ncpus < 1) {
            flux_log (h, LOG_ERR, "Curr job %d incorrectly requires < 1 cpu: %d", curr_job_t->id, curr_job_t->ncpus);
        }
        //De-allocate curr_job_t's resources from the *shadow_rdl
        curr_lwj_job = find_lwj(ctx, curr_job_t->id);
        if (curr_lwj_job->alloc.ncores != curr_job_t->ncpus) {
            flux_log (h, LOG_ERR, "Job %d's ncpus don't match: %d (lwj_job) and %d (job_t)", curr_job_t->id, curr_lwj_job->alloc.ncores, curr_job_t->ncpus);
        }
        release_resources (ctx, *shadow_rdl, uri, curr_lwj_job);

        shadow_free_cores += curr_job_t->ncpus;
        *shadow_time = curr_job_t->start_time + curr_job_t->time_limit;

        curr_job_t = zlist_next (running_jobs);
        rdl_destroy (shadow_free_rdl);
        shadow_free_rdl = get_free_subset (*shadow_rdl, "core");
    }

    rdl_destroy (shadow_free_rdl);
}

//Determines if a job is eligible for backfilling or not
//If it is, attempts to schedule it
static bool backfill_job (struct rdl *rdl, struct rdl *shadow_rdl, struct rdl *free_rdl,
                          struct rdl *shadow_free_rdl, const char *uri, flux_lwj_t *job,
                          int64_t *curr_free_cores, int64_t *shadow_free_cores,
                          double shadow_time, double sim_time)
{
    bool terminates_before_shadow_time = ((sim_time + job->req.walltime) < shadow_time);

    flux_log (h, LOG_DEBUG, "backfill info - term_before_shadow_time: %d, curr_free_cores: %ld, job_req_cores: %d", terminates_before_shadow_time, *curr_free_cores, job->req.ncores);

    if (*curr_free_cores < job->req.ncores) {
        return false;
    } else if (terminates_before_shadow_time) {
        //Job ends before reserved job starts, and we have enough cores currently to schedule it
        if (schedule_job (ctx, rdl, free_rdl, uri, *curr_free_cores, job)) {
            *curr_free_cores -= job->req.ncores;
            return true;
        }
    } else {
        //Job ends after reserved job starts, make sure we will have
        //enough free resources (cores + IO) at shadow time to not
        //delay the reserved job
        struct rdl_accumulator *accum = NULL;
        if (schedule_job_without_update(shadow_rdl, shadow_free_rdl, uri,
                                        *shadow_free_cores, job, &accum))
        {
            if (schedule_job (ctx, rdl, free_rdl, uri, *curr_free_cores, job)) {
                *shadow_free_cores -= job->alloc.ncores;
                *curr_free_cores -= job->alloc.ncores;
                return true;
            } else {
                job->rdl = rdl_accumulator_copy (accum);
                release_resources(ctx, shadow_rdl, uri, job);
                rdl_destroy (job->rdl);
                rdl_accumulator_destroy (accum);
                return false;
            }
        }
    }

    return false;
}


int schedule_jobs (ctx_t *ctx, double sim_time)
{
    flux_lwj_t *curr_job = NULL, *curr_lwj_job = NULL, *reserved_job = NULL;
    job_t *curr_job_t = NULL;
    kvsdir_t *curr_kvs_dir;
    int rc = 0, job_scheduled = 1;
    double shadow_time = -1;
    int64_t curr_free_cores = -1, shadow_free_cores = -1;
    struct rdl *rdl = ctx->rdl, *free_rdl = NULL, *shadow_rdl = NULL, *shadow_free_rdl = NULL;
    zlist_t *jobs = ctx->p_queue, *queued_jobs = zlist_dup (jobs), *running_jobs = zlist_new ();
    char *uri = ctx->uri;

    zlist_sort(queued_jobs, job_compare_t);

    free_rdl = get_free_subset (rdl, "core");
    if (free_rdl) {
        curr_free_cores = get_free_count (free_rdl, uri, "core");
    } else {
        flux_log (h, LOG_DEBUG, "get_free_subset returned nothing, setting curr_free_cores = 0");
        curr_free_cores = 0;
    }

    //Schedule all jobs at the front of the queue
    curr_job = zlist_first (queued_jobs);
    while (job_scheduled && curr_job) {
		if (curr_job->state == j_unsched) {
			job_scheduled = schedule_job (ctx, rdl, free_rdl, uri, curr_free_cores, curr_job);
            if (job_scheduled) {
                rc += 1;
                curr_free_cores -= curr_job->alloc.ncores;
                if (kvs_get_dir (h,  &curr_kvs_dir, "lwj.%ld", curr_job->lwj_id)) {
                    flux_log (h, LOG_ERR, "lwj.%ld kvsdir not found", curr_job->lwj_id);
                } else {
                    curr_job_t = pull_job_from_kvs (curr_kvs_dir);
                    if (curr_job_t->start_time == 0)
                        curr_job_t->start_time = sim_time;
                    zlist_append (running_jobs, curr_job_t);
                }
            } else {
                reserved_job = curr_job;
            }
		}
        curr_job = zlist_next (queued_jobs);
    }

    //reserved job is now set, start backfilling
    if (reserved_job && curr_job) {
        curr_lwj_job = zlist_first (ctx->r_queue);
        while (curr_lwj_job != NULL) {
            if (kvs_get_dir (h,  &curr_kvs_dir, "lwj.%ld", curr_lwj_job->lwj_id)) {
                flux_log (h, LOG_ERR, "lwj.%ld kvsdir not found", curr_lwj_job->lwj_id);
            } else {
                curr_job_t = pull_job_from_kvs (curr_kvs_dir);
                if (curr_job_t->start_time == 0)
                    curr_job_t->start_time = sim_time;
                zlist_append (running_jobs, curr_job_t);
            }
            curr_lwj_job = zlist_next (ctx->r_queue);
        }
        calculate_shadow_info (reserved_job, rdl, uri, running_jobs, &shadow_rdl, &shadow_time);

        shadow_free_rdl = get_free_subset (shadow_rdl, "core");
        if (shadow_free_rdl) {
            shadow_free_cores = get_free_count (shadow_free_rdl, uri, "core");
        } else {
            flux_log (h, LOG_DEBUG, "get_free_subset returned nothing, setting shadow_free_cores = 0");
            shadow_free_cores = 0;
        }

        flux_log(h, LOG_DEBUG, "Job %ld has the reservation - shadow time: %f, shadow free cores: %ld", reserved_job->lwj_id, shadow_time, shadow_free_cores);

        //NOTE: the shadow_rdl and the rdl will diverge due to
        //differences in the order that the jobs are scheduled.  In
        //the case of "rdl", all backfilled jobs are scheduled first,
        //then the reserved job. In the case of "shadow_rdl", the
        //reserved job is scheduled first, then the backfilled jobs.
        while (curr_job && shadow_time >= 0 && curr_free_cores > 0) {
            if (curr_job->state == j_unsched) {
                if (backfill_job(rdl, shadow_rdl, free_rdl, shadow_free_rdl, uri, curr_job,
                                 &curr_free_cores, &shadow_free_cores, shadow_time, sim_time))
                {
                    rc += 1;
                    flux_log(h, LOG_DEBUG, "Job %ld was backfilled.", curr_job->lwj_id);
                } else {
                    flux_log(h, LOG_DEBUG, "Job %ld was not backfillied", curr_job->lwj_id);
                }
            }
            curr_job = zlist_next (queued_jobs);
        }
        rdl_destroy (shadow_rdl);
        rdl_destroy (shadow_free_rdl);
    }

	flux_log (h, LOG_DEBUG, "Finished iterating over the queued_jobs list");
    //Cleanup
    if (free_rdl)
        rdl_destroy (free_rdl);
    curr_job_t = zlist_pop (running_jobs);
    while (curr_job_t != NULL) {
        free_job (curr_job_t);
        curr_job_t = zlist_pop (running_jobs);
    }
    zlist_destroy (&running_jobs);
    zlist_destroy (&queued_jobs);
    return rc;
}


static struct flux_msghandler htab[] = {
    { FLUX_MSGTYPE_EVENT,   "sim.start",     start_cb },
    { FLUX_MSGTYPE_REQUEST, "sim_sched.trigger", trigger_cb },
    { FLUX_MSGTYPE_REQUEST, "sim_sched.lwj-watch",  newlwj_rpc },
    FLUX_MSGHANDLER_TABLE_END,
};

int mod_main (flux_t p, int argc, char **argv)
{
    zhash_t *args = zhash_fromargv (argc, argv);

    h = p;
    ctx = getctx (h);

    return init_and_start_scheduler(h, ctx, args, htab);
}

MOD_NAME ("sim_sched");

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
