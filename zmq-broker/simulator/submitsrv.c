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


static const char const *module_name = "submit";
//static flux_t h = NULL;

//Request to join the simulation
int send_join_request(flux_t h)
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
int send_reply_request(flux_t h, sim_state_t *sim_state)
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

//Based on the sim_time, schedule any jobs that need to be scheduled
//Next, add an event timer for the scheduler to the sim_state
//Finally, updated the submit event timer with the next submit time
int schedule_next_job(flux_t h, sim_state_t *sim_state)
{
	zhash_t * timers = sim_state->timers;
	JSON o = Jnew();
	double *new_mod_time;
	JSON response;
	int64_t new_jobid;
	kvsdir_t dir;

    //Send "job.create" and wait for jobid in response
	Jadd_int (o, "nnodes", 1);
	Jadd_int (o, "ntasks", 1);
	Jadd_double (o, "sim_time", sim_state->sim_time);

	response = flux_rpc (h, o, "job.create");
	Jget_int64 (response, "jobid", &new_jobid);

	//Update lwj.%jobid%'s state in the kvs to "submitted"
	if (kvs_get_dir (h, &dir, "lwj.%lu", new_jobid) < 0)
        err_exit ("kvs_get_dir (id=%lu)", new_jobid);
	kvsdir_put_string (dir, "state", "submitted");
	kvs_commit (h);

	//Update event timers in reply (submit and sched)
	new_mod_time = (double *) zhash_lookup (timers, module_name);
	*new_mod_time = sim_state->sim_time + 2;
	flux_log (h, LOG_DEBUG, "'scheduled' the next job, next submit event will occur at %f", *new_mod_time);
	new_mod_time = (double *) zhash_lookup (timers, "sched");
	if (new_mod_time != NULL)
		*new_mod_time = sim_state->sim_time + DBL_MIN;
	flux_log (h, LOG_DEBUG, "added a sched timer that will occur at %f", *new_mod_time);

	//Cleanup
	Jput (o);
	return 0;
}

//Received an event that a simulation is starting
static int start_cb (flux_t h, int typemask, zmsg_t **zmsg, void *arg)
{
	flux_log(h, LOG_DEBUG, "received a start event");
	if (send_join_request (h) < 0){
		flux_log (h, LOG_ERR, "submit module failed to register with sim module");
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

//Handle trigger requests from the sim module ("submit.trigger")
static int trigger_cb (flux_t h, int typemask, zmsg_t **zmsg, void *arg)
{
	JSON o;
	const char *json_string;
	sim_state_t *sim_state;
	char *tag;

	if (cmb_msg_decode (*zmsg, &tag, &o) < 0 || o == NULL){
		flux_log (h, LOG_ERR, "%s: bad message", __FUNCTION__);
		Jput (o);
		return -1;
	}

//Logging
	json_string = Jtostr (o);
	flux_log(h, LOG_DEBUG, "received a trigger (%s): %s", tag, json_string);

//Handle the trigger
	sim_state = json_to_sim_state (o);
	schedule_next_job (h, sim_state);
	send_reply_request (h, sim_state);

//Cleanup
	free_simstate (sim_state);
	Jput (o);
	zmsg_destroy (zmsg);
	return 0;
}

static msghandler_t htab[] = {
    { FLUX_MSGTYPE_EVENT,   "sim.start",        start_cb },
    { FLUX_MSGTYPE_REQUEST, "submit.trigger",   trigger_cb },
};
const int htablen = sizeof (htab) / sizeof (htab[0]);

int mod_main(flux_t h, zhash_t *args)
{
	if (flux_rank (h) != 0) {
		flux_log (h, LOG_ERR, "submit module must only run on rank 0");
		return -1;
	}
	flux_log (h, LOG_INFO, "submit comms module starting");

	if (flux_event_subscribe (h, "sim.start") < 0){
        flux_log (h, LOG_ERR, "subscribing to event: %s", strerror (errno));
		return -1;
	}
	if (flux_msghandler_addvec (h, htab, htablen, NULL) < 0) {
		flux_log (h, LOG_ERR, "flux_msghandler_add: %s", strerror (errno));
		return -1;
	}
	if (flux_reactor_start (h) < 0) {
		flux_log (h, LOG_ERR, "flux_reactor_start: %s", strerror (errno));
		return -1;
	}

	return 0;
}

MOD_NAME("submit");
