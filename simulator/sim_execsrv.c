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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <flux/core.h>

#include "src/common/libutil/log.h"
#include "src/common/libutil/shortjansson.h"
#include "src/common/libutil/xzmalloc.h"
#include "simulator.h"
#include "rdl.h"

static const char *module_name = "sim_exec";

typedef struct {
    sim_state_t *sim_state;
    zlist_t *queued_events;  // holds int *
    zlist_t *running_jobs;  // holds job_t *
    flux_t *h;
    double prev_sim_time;
    struct rdllib *rdllib;
    struct rdl *rdl;
} ctx_t;

#if SIMEXEC_IO
static double determine_io_penalty (double job_bandwidth, double min_bandwidth);
static double *get_job_min_from_hash (zhash_t *job_hash, int job_id);
#endif

static void freectx (void *arg)
{
    ctx_t *ctx = arg;
    free_simstate (ctx->sim_state);

    while (zlist_size (ctx->queued_events) > 0)
        free (zlist_pop (ctx->queued_events));
    zlist_destroy (&ctx->queued_events);

    while (zlist_size (ctx->running_jobs) > 0)
        free_job (zlist_pop (ctx->running_jobs));
    zlist_destroy (&ctx->running_jobs);

    rdllib_close (ctx->rdllib);
    free (ctx->rdl);
    free (ctx);
}

static ctx_t *getctx (flux_t *h)
{
    ctx_t *ctx = (ctx_t *)flux_aux_get (h, "sim_exec");

    if (!ctx) {
        ctx = xzmalloc (sizeof (*ctx));
        ctx->h = h;
        ctx->sim_state = NULL;
        ctx->queued_events = zlist_new ();
        ctx->running_jobs = zlist_new ();
        ctx->prev_sim_time = 0;
        ctx->rdllib = rdllib_open ();
        ctx->rdl = NULL;
        flux_aux_set (h, "simsrv", ctx, freectx);
    }

    return ctx;
}

// Given the kvs dir of a job, change the state of the job and
// timestamp the change
static int update_job_state (ctx_t *ctx,
                             int64_t jobid,
                             kvsdir_t *kvs_dir,
                             job_state_t new_state,
                             double update_time)
{
    char *timer_key = NULL;

    switch (new_state) {
    case J_STARTING: timer_key = "starting_time"; break;
    case J_RUNNING: timer_key = "running_time"; break;
    case J_COMPLETE: timer_key = "complete_time"; break;
    default: break;
    }
    if (timer_key == NULL) {
        flux_log (ctx->h, LOG_ERR, "Unknown state %d", (int) new_state);
        return -1;
    }

    json_t *jcb = Jnew ();
    json_t *o = Jnew ();

    Jadd_int64 (o, JSC_STATE_PAIR_NSTATE, (int64_t) new_state);
    Jadd_obj (jcb, JSC_STATE_PAIR, o);
    jsc_update_jcb(ctx->h, jobid, JSC_STATE_PAIR, Jtostr (jcb));

    kvsdir_put_double (kvs_dir, timer_key, update_time);
    kvs_commit (ctx->h);

    Jput (jcb);
    Jput (o);

    return 0;
}

static double calc_curr_progress (job_t *job, double sim_time)
{
    double time_passed = sim_time - job->start_time + .000001;
    double time_necessary = job->execution_time + job->io_time;
    return time_passed / time_necessary;
}

// Calculate when the next job is going to terminate assuming no new jobs are
// added
static double determine_next_termination (ctx_t *ctx,
                                          double curr_time,
                                          zhash_t *job_hash)
{
    double next_termination = -1, curr_termination = -1;
#if SIMEXEC_IO
    double projected_future_io_time, job_io_penalty, computation_time_remaining;
    double *job_min_bandwidth;
#endif
    zlist_t *running_jobs = ctx->running_jobs;
    job_t *job = zlist_first (running_jobs);

    while (job != NULL) {
        if (job->start_time <= curr_time) {
            curr_termination =
                job->start_time + job->execution_time + job->io_time;
#if SIMEXEC_IO
            computation_time_remaining =
                job->execution_time
                - ((curr_time - job->start_time) - job->io_time);
            job_min_bandwidth = get_job_min_from_hash (job_hash, job->id);
            job_io_penalty =
                determine_io_penalty (job->io_rate, *job_min_bandwidth);
            projected_future_io_time =
                (computation_time_remaining)*job_io_penalty;
            curr_termination += projected_future_io_time;
#endif
            if (curr_termination < next_termination || next_termination < 0) {
                next_termination = curr_termination;
            }
        }
        job = zlist_next (running_jobs);
    }

    return next_termination;
}

// Set the timer for the given module
static int set_event_timer (ctx_t *ctx, char *mod_name, double timer_value)
{
    double *event_timer = zhash_lookup (ctx->sim_state->timers, mod_name);
    if (timer_value > 0 && (timer_value < *event_timer || *event_timer < 0)) {
        *event_timer = timer_value;
        flux_log (ctx->h,
                  LOG_DEBUG,
                  "next %s event set to %f",
                  mod_name,
                  *event_timer);
    }
    return 0;
}

// Update sched timer as necessary (to trigger an event in sched)
// Also change the state of the job in the KVS
static int complete_job (ctx_t *ctx, job_t *job, double completion_time)
{
    flux_t *h = ctx->h;

    flux_log (h, LOG_INFO, "Job %d completed", job->id);

    update_job_state (ctx, job->id, job->kvs_dir, J_COMPLETE, completion_time);
    set_event_timer (ctx, "sched", ctx->sim_state->sim_time + .00001);
    kvsdir_put_double (job->kvs_dir, "complete_time", completion_time);
    kvsdir_put_double (job->kvs_dir, "io_time", job->io_time);

    kvs_commit (h);
    free_job (job);

    return 0;
}

// Remove completed jobs from the list of running jobs
// Update sched timer as necessary (to trigger an event in sched)
// Also change the state of the job in the KVS
static int handle_completed_jobs (ctx_t *ctx)
{
    double curr_progress;
    zlist_t *running_jobs = ctx->running_jobs;
    job_t *job = NULL;
    int num_jobs = zlist_size (running_jobs);
    double sim_time = ctx->sim_state->sim_time;

    // print_next_completing (running_jobs, ctx);

    while (num_jobs > 0) {
        job = zlist_pop (running_jobs);
        if (job->execution_time > 0) {
            curr_progress = calc_curr_progress (job, ctx->sim_state->sim_time);
        } else {
            curr_progress = 1;
            flux_log (ctx->h,
                      LOG_DEBUG,
                      "handle_completed_jobs found a job (%d) with execution "
                      "time <= 0 (%f), setting progress = 1",
                      job->id,
                      job->execution_time);
        }
        if (curr_progress < 1) {
            zlist_append (running_jobs, job);
        } else {
            flux_log (ctx->h,
                      LOG_DEBUG,
                      "handle_completed_jobs found a completed job");
            complete_job (ctx, job, sim_time);
        }
        num_jobs--;
    }

    return 0;
}

#if SIMEXEC_IO
static int64_t get_alloc_bandwidth (struct resource *r)
{
    int64_t alloc_bw;
    if (rdl_resource_get_int (r, "alloc_bw", &alloc_bw) == 0) {
        return alloc_bw;
    } else {  // something got messed up, set it to zero and return zero
        rdl_resource_set_int (r, "alloc_bw", 0);
        return 0;
    }
}

static int64_t get_max_bandwidth (struct resource *r)
{
    int64_t max_bw;
    rdl_resource_get_int (r, "max_bw", &max_bw);
    return max_bw;
}

#if CZMQ_VERSION < CZMQ_MAKE_VERSION(3, 0, 1)
// Compare two resources based on their alloc bandwidth
// Return true if they should be swapped
// AKA r1 has more alloc bandwidth than r2
bool compare_resource_alloc_bw (void *item1, void *item2)
{
    struct resource *r1 = (struct resource *)item1;
    struct resource *r2 = (struct resource *)item2;
    return get_alloc_bandwidth (r1) > get_alloc_bandwidth (r2);
}
#else
// Compare two resources based on their alloc bandwidth
// Return > 0 if res1 has more alloc bandwidth than res2
//        < 0 if res1 has less alloc bandwidth than res2
//        = 0 if bandwidths are equivalent
int compare_resource_alloc_bw (void *item1, void *item2)
{
    struct resource *r1 = (struct resource *)item1;
    struct resource *r2 = (struct resource *)item2;
    double bw1 = get_alloc_bandwidth (r1);
    double bw2 = get_alloc_bandwidth (r2);
    if (bw1 == bw2)
        return 0;
    else if (bw1 > bw2)
        return 1;
    else
        return (-1);
}
#endif /* CZMQ_VERSION > 3.0.0 */

static double *get_job_min_from_hash (zhash_t *job_hash, int job_id)
{
    char job_id_str[100];
    sprintf (job_id_str, "%d", job_id);
    return (double *)zhash_lookup (job_hash, job_id_str);
}

static void determine_all_min_bandwidth_helper (struct resource *r,
                                                double curr_min_bandwidth,
                                                zhash_t *job_hash)
{
    struct resource *curr_child;
    int64_t job_id;
    double total_requested_bandwidth, curr_average_bandwidth,
        child_alloc_bandwidth, total_used_bandwidth, this_max_bandwidth,
        num_children, this_alloc_bandwidth;
    json_t *o;
    zlist_t *child_list;
    const char *type = NULL;

    // Check if leaf node in hierarchy (base case)
    rdl_resource_iterator_reset (r);
    curr_child = rdl_resource_next_child (r);
    if (curr_child == NULL) {
        // Check if allocated to a job
        if (rdl_resource_get_int (r, "lwj", &job_id) == 0) {
            // Determine which is less, the bandwidth currently available to
            // this resource, or the bandwidth allocated to it by the job
            // This assumes that jobs cannot share leaf nodes in the hierarchy
            this_alloc_bandwidth = get_alloc_bandwidth (r);
            curr_min_bandwidth = (curr_min_bandwidth < this_alloc_bandwidth)
                                     ? curr_min_bandwidth
                                     : this_alloc_bandwidth;
            double *job_min_bw = get_job_min_from_hash (job_hash, job_id);
            if (job_min_bw != NULL && curr_min_bandwidth < *job_min_bw) {
                *job_min_bw = curr_min_bandwidth;
            }  // if job_min_bw is NULL, the tag still exists in the RDL, but
               // the job completed
        }
        return;
    }  // else

    // Sum the bandwidths of the parent's children
    total_requested_bandwidth = 0;
    child_list = zlist_new ();
    while (curr_child != NULL) {
        o = rdl_resource_json (curr_child);
        Jget_str (o, "type", &type);
        // TODO: clean up this hardcoded value, should go away once we switch to
        // the real
        // rdl implementation (storing a bandwidth resource at every level)
        if (strcmp (type, "memory") != 0) {
            total_requested_bandwidth += get_alloc_bandwidth (curr_child);
            zlist_append (child_list, curr_child);
        }
        Jput (o);
        curr_child = rdl_resource_next_child (r);
    }
    rdl_resource_iterator_reset (r);

    // Sort child list based on alloc bw
    zlist_sort (child_list, compare_resource_alloc_bw);

    // const char *resource_string = Jtostr(o);
    // Loop over all of the children
    this_max_bandwidth = get_max_bandwidth (r);
    total_used_bandwidth = (total_requested_bandwidth > this_max_bandwidth)
                               ? this_max_bandwidth
                               : total_requested_bandwidth;
    total_used_bandwidth = (total_used_bandwidth > curr_min_bandwidth)
                               ? curr_min_bandwidth
                               : total_used_bandwidth;
    while (zlist_size (child_list) > 0) {
        // Determine the amount of bandwidth to allocate to each child
        num_children = zlist_size (child_list);
        curr_average_bandwidth = (total_used_bandwidth / num_children);
        curr_child = (struct resource *)zlist_pop (child_list);
        child_alloc_bandwidth = get_alloc_bandwidth (curr_child);
        if (child_alloc_bandwidth > 0) {
            if (child_alloc_bandwidth > curr_average_bandwidth)
                child_alloc_bandwidth = curr_average_bandwidth;

            // Subtract the allocated bandwidth from the parent's total
            total_used_bandwidth -= child_alloc_bandwidth;
            // Recurse on the child
            determine_all_min_bandwidth_helper (curr_child,
                                                child_alloc_bandwidth,
                                                job_hash);
        }
        rdl_resource_destroy (curr_child);
    }

    // Cleanup
    zlist_destroy (
        &child_list);  // no need to rdl_resource_destroy, done in above loop

    return;
}

static zhash_t *determine_all_min_bandwidth (struct rdl *rdl,
                                             zlist_t *running_jobs)
{
    double root_bw;
    double *curr_value = NULL;
    struct resource *root = NULL;
    job_t *curr_job = NULL;
    char job_id_str[100];
    zhash_t *job_hash = zhash_new ();

    root = rdl_resource_get (rdl, "default");
    root_bw = get_max_bandwidth (root);

    curr_job = zlist_first (running_jobs);
    while (curr_job != NULL) {
        curr_value = (double *)malloc (sizeof (double));
        *curr_value = root_bw;
        sprintf (job_id_str, "%d", curr_job->id);
        zhash_insert (job_hash, job_id_str, curr_value);
        zhash_freefn (job_hash, job_id_str, free);
        curr_job = zlist_next (running_jobs);
    }

    determine_all_min_bandwidth_helper (root, root_bw, job_hash);

    return job_hash;
}

static double determine_io_penalty (double job_bandwidth, double min_bandwidth)
{
    double io_penalty;

    if (job_bandwidth < min_bandwidth || min_bandwidth == 0) {
        return 0;
    }

    // Determine the penalty (needed rate / actual rate) - 1
    io_penalty = (job_bandwidth / min_bandwidth) - 1;

    return io_penalty;
}
#endif

// Model io contention that occurred between previous event and the
// curr sim time. Remove completed jobs from the list of running jobs
static int advance_time (ctx_t *ctx, zhash_t *job_hash)
{
    // TODO: Make this not static? (pass it in?, store it in ctx?)
    static double curr_time = 0;

    job_t *job = NULL;
    int num_jobs = -1;
    double next_event = -1, next_termination = -1, curr_progress = -1
#if SIMEXEC_IO
        ,io_penalty = 0, io_percentage = 0;
    double *job_min_bandwidth = NULL;
#else
    ;
#endif

    zlist_t *running_jobs = ctx->running_jobs;
    double sim_time = ctx->sim_state->sim_time;

    while (curr_time < sim_time) {
        num_jobs = zlist_size (running_jobs);
        if (num_jobs == 0) {
            curr_time = sim_time;
            break;
        }
        next_termination =
            determine_next_termination (ctx, curr_time, job_hash);
        next_event = ((sim_time < next_termination) || (next_termination < 0))
                         ? sim_time
                         : next_termination;  // min of the two
        while (num_jobs > 0) {
            job = zlist_pop (running_jobs);
            if (job->start_time <= curr_time) {
#if SIMEXEC_IO
                // Get the minimum bandwidth between a resource in the job and
                // the pfs
                job_min_bandwidth = get_job_min_from_hash (job_hash, job->id);
                io_penalty =
                    determine_io_penalty (job->io_rate, *job_min_bandwidth);
                io_percentage = (io_penalty / (io_penalty + 1));
                job->io_time += (next_event - curr_time) * io_percentage;
#endif
                curr_progress = calc_curr_progress (job, next_event);
                if (curr_progress < 1)
                    zlist_append (running_jobs, job);
                else
                    complete_job (ctx, job, next_event);
            } else {
                zlist_append (running_jobs, job);
            }
            num_jobs--;
        }
        curr_time = next_event;
    }

    return 0;
}

// Take all of the scheduled job events that were queued up while we
// weren't running and add those jobs to the set of running jobs. This
// also requires switching their state in the kvs (to trigger events
// in the scheduler)
static int handle_queued_events (ctx_t *ctx)
{
    job_t *job = NULL;
    int *jobid = NULL;
    kvsdir_t *kvs_dir;
    flux_t *h = ctx->h;
    zlist_t *queued_events = ctx->queued_events;
    zlist_t *running_jobs = ctx->running_jobs;
    double sim_time = ctx->sim_state->sim_time;

    while (zlist_size (queued_events) > 0) {
        jobid = zlist_pop (queued_events);
        if (!(kvs_dir = job_kvsdir (ctx->h, *jobid)))
            log_err_exit ("job_kvsdir (id=%d)", *jobid);
        job = pull_job_from_kvs (*jobid, kvs_dir);
        if (update_job_state (ctx, *jobid, kvs_dir, J_STARTING, sim_time) < 0) {
            flux_log (h,
                      LOG_ERR,
                      "failed to set job %d's state to starting",
                      *jobid);
            return -1;
        }
        if (update_job_state (ctx, *jobid, kvs_dir, J_RUNNING, sim_time) < 0) {
            flux_log (h,
                      LOG_ERR,
                      "failed to set job %d's state to running",
                      *jobid);
            return -1;
        }
        flux_log (h,
                  LOG_INFO,
                  "job %d's state to starting then running",
                  *jobid);
        job->start_time = ctx->sim_state->sim_time;
        zlist_append (running_jobs, job);
    }

    return 0;
}

// Received an event that a simulation is starting
static void start_cb (flux_t *h,
                      flux_msg_handler_t *w,
                      const flux_msg_t *msg,
                      void *arg)
{
    flux_log (h, LOG_DEBUG, "received a start event");
    if (send_join_request (h, module_name, -1) < 0) {
        flux_log (h, LOG_ERR, "failed to register with sim module");
        return;
    }
    flux_log (h, LOG_DEBUG, "sent a join request");

    if (flux_event_unsubscribe (h, "sim.start") < 0) {
        flux_log (h, LOG_ERR, "failed to unsubscribe from \"sim.start\"");
    } else {
        flux_log (h, LOG_DEBUG, "unsubscribed from \"sim.start\"");
    }
}

// Handle trigger requests from the sim module ("sim_exec.trigger")
static void trigger_cb (flux_t *h,
                        flux_msg_handler_t *w,
                        const flux_msg_t *msg,
                        void *arg)
{
    json_t *o = NULL;
    const char *json_str = NULL;
    double next_termination = -1;
    zhash_t *job_hash = NULL;
    ctx_t *ctx = (ctx_t *)arg;

    if (flux_msg_get_payload_json (msg, &json_str) < 0 || json_str == NULL
        || !(o = Jfromstr (json_str))) {
        flux_log (h, LOG_ERR, "%s: bad message", __FUNCTION__);
        return;
    }

    // Logging
    flux_log (h,
              LOG_DEBUG,
              "received a trigger (sim_exec.trigger: %s",
              json_str);

    // Handle the trigger
    ctx->sim_state = json_to_sim_state (o);
    handle_queued_events (ctx);
#if SIMEXEC_IO
    job_hash = determine_all_min_bandwidth (ctx->rdl, ctx->running_jobs);
#endif
    advance_time (ctx, job_hash);
    handle_completed_jobs (ctx);
    next_termination =
        determine_next_termination (ctx, ctx->sim_state->sim_time, job_hash);
    set_event_timer (ctx, "sim_exec", next_termination);
    send_reply_request (h, module_name, ctx->sim_state);

    // Cleanup
    free_simstate (ctx->sim_state);
    Jput (o);
    zhash_destroy (&job_hash);
}

static void run_cb (flux_t *h,
                    flux_msg_handler_t *w,
                    const flux_msg_t *msg,
                    void *arg)
{
    const char *topic;
    ctx_t *ctx = (ctx_t *)arg;
    int *jobid = (int *)malloc (sizeof (int));

    if (flux_msg_get_topic (msg, &topic) < 0) {
        flux_log (h, LOG_ERR, "%s: bad message", __FUNCTION__);
        free (jobid);
        return;
    }

    // Logging
    flux_log (h, LOG_DEBUG, "received a request (%s)", topic);

    // Handle Request
    sscanf (topic, "sim_exec.run.%d", jobid);
    zlist_append (ctx->queued_events, jobid);
    flux_log (h, LOG_DEBUG, "queued the running of jobid %d", *jobid);
}

static struct flux_msg_handler_spec htab[] = {
    {FLUX_MSGTYPE_EVENT, "sim.start", start_cb},
    {FLUX_MSGTYPE_REQUEST, "sim_exec.trigger", trigger_cb},
    {FLUX_MSGTYPE_REQUEST, "sim_exec.run.*", run_cb},
    FLUX_MSGHANDLER_TABLE_END,
};

int mod_main (flux_t *h, int argc, char **argv)
{
    ctx_t *ctx = getctx (h);
    uint32_t rank;

    if (flux_get_rank (h, &rank) < 0)
        return -1;
    if (rank != 0) {
        flux_log (h, LOG_ERR, "this module must only run on rank 0");
        return -1;
    }
    flux_log (h, LOG_INFO, "module starting");

    if (flux_event_subscribe (h, "sim.start") < 0) {
        flux_log (h, LOG_ERR, "subscribing to event: %s", strerror (errno));
        return -1;
    }
    if (flux_msg_handler_addvec (h, htab, ctx) < 0) {
        flux_log (h, LOG_ERR, "flux_msg_handler_add: %s", strerror (errno));
        return -1;
    }

    send_alive_request (h, module_name);

    if (flux_reactor_run (flux_get_reactor (h), 0) < 0) {
        flux_log (h, LOG_ERR, "flux_reactor_run: %s", strerror (errno));
        return -1;
    }

    return 0;
}

MOD_NAME ("sim_exec");
