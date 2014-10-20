#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <json.h>
#include <flux/core.h>

#include "src/common/libutil/jsonutil.h"
#include "src/common/libutil/log.h"
#include "src/common/libutil/shortjson.h"
#include "simulator.h"

static const char const *module_name = "submit";
static zlist_t *jobs;  //TODO: remove from "global" scope

//Compare two job_t's based on submit time
//Return true if they should be swapped
//AKA job1 was submitted after job2
bool compare_job_t (void *item1, void *item2)
{
	job_t *job1 = (job_t *) item1;
	job_t *job2 = (job_t *) item2;
	return job1->submit_time > job2->submit_time;
}

//Figure out when the next submit time is
//This assumes the list is sorted by submit time
double get_next_submit_time ()
{
	job_t *job;
	if (zlist_size (jobs) > 0){
		job = zlist_head (jobs);
		return job->submit_time;
	}
	return -1;
}

//Convert the string representation of time in the csv to sec
double convert_time_to_sec (char* time)
{
	int hours, minutes, seconds;
	sscanf (time, "%d:%d:%d", &hours, &minutes, &seconds);
	return (double) ((hours * 3600) + (minutes * 60) + seconds);
}

//Populate a field in the job_t based off a value extracted from the csv
int insert_into_job (job_t *job, char *column_name, char *value)
{
	if (!strcmp (column_name, "JobID")){
		job->id = atoi (value);
	}
	else if (!strcmp (column_name, "User")){
		asprintf (&job->user, "%s", value);
	}
	else if (!strcmp (column_name, "JobName")){
		asprintf (&job->jobname, "%s", value);
	}
	else if (!strcmp (column_name, "Account")){
		asprintf (&job->account, "%s", value);
	}
	else if (!strcmp (column_name, "NNodes")){
		job->nnodes = atoi (value);
	}
	else if (!strcmp (column_name, "NCPUS")){
		job->ncpus = atoi (value);
	}
	else if (!strcmp (column_name, "Timelimit")){
		job->time_limit = convert_time_to_sec (value);
	}
	else if (!strcmp (column_name, "Submit")){
		job->submit_time = atof (value);
	}
	else if (!strcmp (column_name, "Elapsed")){
		job->execution_time = convert_time_to_sec (value);
	}
	else if (!strncmp (column_name, "IORate(MB)", 10)){ //ignore the \n at the end using strncmp
		job->io_rate = atoi (value) * 1024 * 1024; //convert MB to bytes
	}
	return 0;
}

//Take the header string from the csv and tokenize it based on the ","
//Then insert each column name into a zlist
int populate_header (char *header_line, zlist_t *header_list)
{
	char *token_copy, *token;
	token = strtok (header_line, ",");
	while (token != NULL) {
		asprintf (&token_copy, "%s", token);
 		zlist_append (header_list, token_copy);
		token = strtok (NULL, ",");
	}
	return 0;
}

//Populate a list of jobs using the data contained in the csv
//TODO: breakup this function into several smaller functions
int parse_job_csv (flux_t h, char *filename, zlist_t *jobs)
{
	const int MAX_LINE_LEN = 500; //sort of arbitrary, works for my data
	char curr_line [MAX_LINE_LEN]; //current line of the input file
	zlist_t *header = NULL; //column names
	char *fget_rc = NULL; //stores the return code of fgets
	char *token = NULL; //current token from the current line
	char *curr_column = NULL; //current column name (pulled from header)
	job_t *curr_job = NULL; //current job object that we are populating

	FILE *fp = fopen (filename, "r");
	if (fp == NULL){
		flux_log (h, LOG_ERR, "csv failed to open");
		return -1;
	}

	//Populate the header list of column names
	header = zlist_new();
	fget_rc = fgets (curr_line, MAX_LINE_LEN, fp);
	if (fget_rc != NULL && feof (fp) == 0) {
		populate_header (curr_line, header);
		fget_rc = fgets (curr_line, MAX_LINE_LEN, fp);
		curr_column = zlist_first (header);
	}

	//Start making jobs from the actual data
	while (fget_rc != NULL && feof (fp) == 0) {
		curr_job = blank_job();
		token = strtok (curr_line, ",");
		//Walk through even column in record and insert the data into the job
		while (token != NULL) {
			if (curr_column == NULL){
				flux_log (h, LOG_ERR, "column name is NULL");
				return -1;
			}
			insert_into_job (curr_job, curr_column, token);
			token = strtok (NULL, ",");
			curr_column = zlist_next (header);
		}
		zlist_append (jobs, curr_job);
		fget_rc = fgets (curr_line, MAX_LINE_LEN, fp);
		curr_column = zlist_first (header);
	}
	zlist_sort (jobs, compare_job_t);

	//Cleanup
	while (zlist_size (header) > 0)
		free (zlist_pop (header));
	zlist_destroy (&header);
	if (fclose (fp) != 0){
		flux_log (h, LOG_ERR, "csv file failed to close");
		return -1;
	}
	return 0;
}

//Request to join the simulation
int send_join_request (flux_t h)
{
	JSON o = Jnew ();
	Jadd_str (o, "mod_name", module_name);
	Jadd_int (o, "rank", flux_rank (h));
	Jadd_double (o, "next_event", get_next_submit_time());
	if (flux_request_send (h, o, "%s", "sim.join") < 0){
		Jput (o);
		return -1;
	}
	Jput (o);
	return 0;
}

//Reply back to the sim module with the updated sim state (in JSON form)
int send_reply_request (flux_t h, sim_state_t *sim_state)
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
int schedule_next_job (flux_t h, sim_state_t *sim_state)
{
	zhash_t * timers = sim_state->timers;
	JSON o = Jnew();
	double *new_sched_mod_time;
	double *new_submit_mod_time;
	JSON response;
	int64_t new_jobid = -1;
	kvsdir_t dir;
	job_t *job;

	//Get the next job to submit
	//Then craft a "job.create" from the job_t and wait for jobid in response
	job = zlist_pop (jobs);
	if (job == NULL) {
		flux_log (h, LOG_ERR, "no more jobs to submit");
		Jput (o);
		return -1;
	}
	Jadd_int (o, "nnodes", job->nnodes);
	Jadd_int (o, "ntasks", job->ncpus);

	response = flux_rpc (h, o, "job.create");
	Jget_int64 (response, "jobid", &new_jobid);

	//Update lwj.%jobid%'s state in the kvs to "submitted"
	if (kvs_get_dir (h, &dir, "lwj.%lu", new_jobid) < 0)
        err_exit ("kvs_get_dir (id=%lu)", new_jobid);
	kvsdir_put_string (dir, "state", "submitted");
	job->kvs_dir = dir;
	put_job_in_kvs (job);
	kvs_commit (h);

	//Update event timers in reply (submit and sched)
	new_sched_mod_time = (double *) zhash_lookup (timers, "sim_sched");
	if (new_sched_mod_time != NULL)
		*new_sched_mod_time = sim_state->sim_time + .00001;
	flux_log (h, LOG_DEBUG, "added a sim_sched timer that will occur at %f", *new_sched_mod_time);
	new_submit_mod_time = (double *) zhash_lookup (timers, module_name);
	if (get_next_submit_time() > *new_sched_mod_time)
		*new_submit_mod_time = get_next_submit_time();
	else
		*new_submit_mod_time = *new_sched_mod_time + .0001;
	flux_log (h, LOG_INFO, "submitted job %ld (%d in csv)", new_jobid, job->id);
	flux_log (h, LOG_DEBUG, "next submit event will occur at %f", *new_submit_mod_time);

	//Cleanup
	free_job (job);
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

	if (flux_msg_decode (*zmsg, &tag, &o) < 0 || o == NULL){
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
	char *csv_filename;
	if (flux_rank (h) != 0) {
		flux_log (h, LOG_ERR, "submit module must only run on rank 0");
		return -1;
	}
	flux_log (h, LOG_INFO, "submit comms module starting");

	//Get the job data from the csv
	if (!(csv_filename = zhash_lookup (args, "job-csv"))) {
		flux_log (h, LOG_ERR, "job-csv argument is not set");
        return -1;
	}
	jobs = zlist_new ();
	parse_job_csv (h, csv_filename, jobs);
	flux_log (h, LOG_INFO, "submit comms module finished parsing job data");

	if (flux_event_subscribe (h, "sim.start") < 0){
        flux_log (h, LOG_ERR, "subscribing to event: %s", strerror (errno));
		return -1;
	}
	if (flux_msghandler_addvec (h, htab, htablen, NULL) < 0) {
		flux_log (h, LOG_ERR, "flux_msghandler_add: %s", strerror (errno));
		return -1;
	}

	send_alive_request (h, module_name);

	if (flux_reactor_start (h) < 0) {
		flux_log (h, LOG_ERR, "flux_reactor_start: %s", strerror (errno));
		return -1;
	}

	return 0;
}

MOD_NAME("submit");
