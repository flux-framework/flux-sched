#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <czmq.h>
#include <json/json.h>

#include "util.h"
#include "log.h"
#include "zmsg.h"
#include "shortjson.h"
#include "plugin.h"
#include "simulator.h"

static const char const *module_name = "sim_exec";

typedef struct {
	sim_state_t *sim_state;
	zlist_t *queued_events; //holds int *
	zlist_t *running_jobs; //holds job_t *
	flux_t h;
	double prev_sim_time;
} ctx_t;

static void freectx (ctx_t *ctx)
{
	free_simstate (ctx->sim_state);

	while (zlist_size (ctx->queued_events) > 0)
		free (zlist_pop (ctx->queued_events));
	zlist_destroy (&ctx->queued_events);

	while (zlist_size (ctx->running_jobs) > 0)
		free_job (zlist_pop (ctx->running_jobs));
	zlist_destroy (&ctx->running_jobs);

    free (ctx);
}

static ctx_t *getctx (flux_t h)
{
    ctx_t *ctx = (ctx_t *)flux_aux_get (h, "sim_exec");

    if (!ctx) {
        ctx = xzmalloc (sizeof (*ctx));
        ctx->h = h;
		ctx->sim_state = NULL;
		ctx->queued_events = zlist_new ();
		ctx->running_jobs = zlist_new ();
		ctx->prev_sim_time = 0;
        flux_aux_set (h, "simsrv", ctx, (FluxFreeFn)freectx);
    }

    return ctx;
}

//Given the kvs dir of a job, change the state of the job and timestamp the change
static int update_job_state (ctx_t *ctx, kvsdir_t kvs_dir, char* state)
{
	double sim_time = ctx->sim_state->sim_time;
	char *timer_key = NULL;

	asprintf (&timer_key, "%s_time", state);

	kvsdir_put_string (kvs_dir, "state", state);
	kvsdir_put_double (kvs_dir, timer_key, sim_time);
	kvs_commit (ctx->h);

	free (timer_key);
	return 0;
}

//Calculate when the next job is going to terminate assuming no new jobs are added
static double determine_next_termination (ctx_t *ctx)
{
	double next_termination = -1;
	double curr_termination = -1;
	zlist_t *running_jobs = ctx->running_jobs;
	job_t *job = zlist_first (running_jobs);

	while (job != NULL){
		curr_termination = job->start_time + job->execution_time;
		if (curr_termination < next_termination || next_termination < 0){
			next_termination = curr_termination;
		}
		job = zlist_next (running_jobs);
	}

	return next_termination;
}

//Set the timer for the given module
//TODO: move this to simulator.c and make it more universal
static int set_event_timer (ctx_t *ctx, char *mod_name, double timer_value)
{
	double *event_timer = zhash_lookup (ctx->sim_state->timers, mod_name);
	if (timer_value > 0 && (timer_value < *event_timer || *event_timer < 0)){
		*event_timer = timer_value;
		flux_log (ctx->h, LOG_DEBUG, "next %s event set to %f", mod_name, *event_timer);
	}
	return 0;
}

//Remove completed jobs from the list of running jobs
//Update sched timer as necessary (to trigger an event in sched)
//Also change the state of the job in the KVS
static int handle_completed_jobs (ctx_t *ctx)
{
	int curr_progress;
	job_t *job = NULL;
	flux_t h = ctx->h;
	zlist_t *running_jobs = ctx->running_jobs;
	int num_jobs = zlist_size (running_jobs);
	while (num_jobs > 0){
		job = zlist_pop (running_jobs);
		curr_progress = ((double)(ctx->sim_state->sim_time - job->start_time))/(job->execution_time + job->io_time);
		if (curr_progress < 1){
			zlist_append (running_jobs, job);
		} else {
			flux_log (h, LOG_DEBUG, "Job %d completed", job->id);
			update_job_state (ctx, job->kvs_dir, "complete");
			set_event_timer (ctx, "sim_sched", ctx->sim_state->sim_time + DBL_MIN);
			kvsdir_put_double (job->kvs_dir, "io_time", job->io_time);
			free_job (job);
		}
		num_jobs--;
	}

	return 0;
}

//Take all of the scheduled job eventst that were queued up while we weren't running
//and add those jobs to the set of running jobs
//This also requires switching their state in the kvs (to trigger events in the scheudler)
static int handle_queued_events (ctx_t *ctx)
{
	job_t *job = NULL;
	int *jobid = NULL;
	kvsdir_t kvs_dir;
	flux_t h = ctx->h;
	zlist_t *queued_events = ctx->queued_events;
	zlist_t *running_jobs = ctx->running_jobs;

	while (zlist_size (queued_events) > 0){
		jobid = zlist_pop (queued_events);
		if (kvs_get_dir (h, &kvs_dir, "lwj.%d", *jobid) < 0)
			err_exit ("kvs_get_dir (id=%d)", *jobid);
		job = pull_job_from_kvs (kvs_dir);
		if (update_job_state (ctx, kvs_dir, "starting") < 0){
			flux_log (h, LOG_ERR, "failed to set job %d's state to starting", *jobid);
			return -1;
		}
		if (update_job_state (ctx, kvs_dir, "running") < 0){
			flux_log (h, LOG_ERR, "failed to set job %d's state to running", *jobid);
			return -1;
		}
		flux_log (h, LOG_DEBUG, "job %d's state to starting then running", *jobid);
		job->start_time = ctx->sim_state->sim_time;
		zlist_append (running_jobs, job);
	}

	return 0;
}


//Request to join the simulation
static int send_join_request(flux_t h)
{
	JSON o = Jnew ();
	Jadd_str (o, "mod_name", module_name);
	Jadd_int (o, "rank", flux_rank (h));
	Jadd_double (o, "next_event", 100);
	if (flux_request_send (h, o, "%s", "sim.join") < 0){
		Jput (o);
		return -1;
	}
	Jput (o);
	return 0;
}

//Reply back to the sim module with the updated sim state (in JSON form)
static int send_reply_request(flux_t h, sim_state_t *sim_state)
{
	JSON o = sim_state_to_json (sim_state);
	Jadd_bool (o, "event_finished", true);
	if (flux_request_send (h, o, "%s", "sim.reply") < 0){
		Jput (o);
		return -1;
	}
   flux_log(h, LOG_DEBUG, "sent a reply request");
   Jput (o);
   return 0;
}

//Received an event that a simulation is starting
static int start_cb (flux_t h, int typemask, zmsg_t **zmsg, void *arg)
{
	flux_log(h, LOG_DEBUG, "received a start event");
	if (send_join_request (h) < 0){
		flux_log (h, LOG_ERR, "failed to register with sim module");
		return -1;
	}
	flux_log (h, LOG_DEBUG, "sent a join request");

	if (flux_event_unsubscribe (h, "sim.start") < 0){
		flux_log (h, LOG_ERR, "failed to unsubscribe from \"sim.start\"");
		return -1;
	} else {
		flux_log (h, LOG_DEBUG, "unsubscribed from \"sim.start\"");
	}

	//Cleanup
	zmsg_destroy (zmsg);

	return 0;
}

//Handle trigger requests from the sim module ("sim_exec.trigger")
static int trigger_cb (flux_t h, int typemask, zmsg_t **zmsg, void *arg)
{
	JSON o;
	const char *json_string;
	char *tag;
	int next_termination;
	ctx_t *ctx = (ctx_t *) arg;

	if (cmb_msg_decode (*zmsg, &tag, &o) < 0 || o == NULL){
		flux_log (h, LOG_ERR, "%s: bad message", __FUNCTION__);
		Jput (o);
		return -1;
	}

//Logging
	json_string = Jtostr (o);
	flux_log(h, LOG_DEBUG, "received a trigger (%s): %s", tag, json_string);

//Handle the trigger
	ctx->sim_state = json_to_sim_state (o);
	handle_queued_events (ctx);
	handle_completed_jobs (ctx);
	next_termination = determine_next_termination (ctx);
	set_event_timer (ctx, "sim_exec", next_termination);
	send_reply_request (h, ctx->sim_state);

//Cleanup
	free_simstate (ctx->sim_state);
	Jput (o);
	zmsg_destroy (zmsg);
	return 0;
}

static int run_cb (flux_t h, int typemask, zmsg_t **zmsg, void *arg)
{
	JSON o;
	char *tag;
	ctx_t *ctx = (ctx_t *) arg;
	int *jobid = (int *) malloc (sizeof (int));

	if (cmb_msg_decode (*zmsg, &tag, &o) < 0 || o == NULL){
		flux_log (h, LOG_ERR, "%s: bad message", __FUNCTION__);
		Jput (o);
		return -1;
	}

//Logging
	flux_log(h, LOG_DEBUG, "received a request (%s)", tag);

//Handle Request
	sscanf (tag, "sim_exec.run.%d", jobid);
	zlist_append (ctx->queued_events, jobid);
	flux_log(h, LOG_DEBUG, "queued the running of jobid %d", *jobid);

//Cleanup
	Jput (o);
	zmsg_destroy (zmsg);
	return 0;
}

static msghandler_t htab[] = {
    { FLUX_MSGTYPE_EVENT,   "sim.start",        start_cb },
    { FLUX_MSGTYPE_REQUEST, "sim_exec.trigger",   trigger_cb },
    { FLUX_MSGTYPE_REQUEST, "sim_exec.run.*",   run_cb },
};
const int htablen = sizeof (htab) / sizeof (htab[0]);

int mod_main(flux_t h, zhash_t *args)
{
	ctx_t *ctx = getctx (h);
	if (flux_rank (h) != 0) {
		flux_log (h, LOG_ERR, "this module must only run on rank 0");
		return -1;
	}
	flux_log (h, LOG_INFO, "module starting");

	if (flux_event_subscribe (h, "sim.start") < 0){
        flux_log (h, LOG_ERR, "subscribing to event: %s", strerror (errno));
		return -1;
	}
	if (flux_msghandler_addvec (h, htab, htablen, ctx) < 0) {
		flux_log (h, LOG_ERR, "flux_msghandler_add: %s", strerror (errno));
		return -1;
	}
	if (flux_reactor_start (h) < 0) {
		flux_log (h, LOG_ERR, "flux_reactor_start: %s", strerror (errno));
		return -1;
	}

	return 0;
}


MOD_NAME("sim_exec");
