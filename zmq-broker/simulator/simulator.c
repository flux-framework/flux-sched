#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <czmq.h>
#include <json/json.h>
//#include <json/json_object_iterator.h>
#include <czmq.h>

#include "flux.h"
#include "util.h"
#include "log.h"
#include "shortjson.h"
#include "simulator.h"

sim_state_t *new_simstate ()
{
	sim_state_t *sim_state = (sim_state_t*) malloc (sizeof (sim_state_t));
	sim_state->timers = zhash_new();
	sim_state->sim_time = 0;
	return sim_state;
}

//A zhash_foreach function that will print all the keys/values in the hashtable
int print_values (const char *key, void *item, void *argument)
{
	int *value = (int *) item;
	fprintf (stderr, "Key: %s\tValue: %d\n", key, *value);
	return 0;
}

//A zhash_foreach function that will set the free_fn on all the values in the hashtable
int set_freefn (const char *key, void *item, void *argument)
{
	zhash_t *self = (zhash_t *) argument;
	zhash_freefn (self, key, free);
	return 0;
}

void free_simstate (sim_state_t* sim_state)
{
	if (sim_state != NULL){
		if (sim_state->timers != NULL){
			zhash_destroy (& (sim_state->timers));
		}
		free (sim_state);
		return;
	}
	fprintf (stderr, "free_simstate called on a NULL pointer\n");
}

static int add_timers_to_json (const char *key, void *item, void *argument)
{
	JSON o = argument;
	double *event_time = (double *) item;

	if (event_time != NULL)
		Jadd_double (o, key, *event_time);
	else
		Jadd_double (o, key, -1);
	return 0;
}

JSON sim_state_to_json (sim_state_t *sim_state)
{
	JSON o = Jnew ();
	JSON event_timers = Jnew ();

	zhash_foreach (sim_state->timers, add_timers_to_json, event_timers);

	//build the main json obg
	Jadd_double (o, "sim_time", sim_state->sim_time);
	Jadd_obj (o, "event_timers", event_timers);

	return o;
}

static void add_timers_to_hash (JSON o, zhash_t *hash)
{
	JSON value;
	const char *key;
	double *event_time;
	struct json_object_iterator iter = json_object_iter_begin (o);
	struct json_object_iterator iter_end = json_object_iter_end (o);
	while (!json_object_iter_equal (&iter, &iter_end)){
		key = json_object_iter_peek_name (&iter);
		value = json_object_iter_peek_value (&iter);
		event_time = (double *) malloc (sizeof (double));
		*event_time = json_object_get_double (value);

		//Insert key,value pair into sim_state hashtable
		zhash_insert (hash, key, event_time);

		json_object_iter_next (&iter);
	}
}

sim_state_t* json_to_sim_state(JSON o)
{
	sim_state_t *sim_state = new_simstate();
	JSON event_timers;

	Jget_double (o, "sim_time", &sim_state->sim_time);
	if (Jget_obj (o, "event_timers", &event_timers)){
		add_timers_to_hash (event_timers, sim_state->timers);
	}

	return sim_state;
}

void free_job (job_t *job)
{
	free (job->user);
	free (job->jobname);
	free (job->account);
	kvsdir_destroy (job->kvs_dir);
	free (job);
}

job_t *blank_job ()
{
	job_t *job = malloc (sizeof (job_t));
	job->id = -1;
	job->user = NULL;
	job->jobname = NULL;
	job->account = NULL;
	job->submit_time = 0;
	job->start_time = 0;
	job->execution_time = 0;
	job->io_time = 0;
	job->time_limit = 0;
	job->nnodes = 0;
	job->ncpus = 0;
	job->kvs_dir = NULL;
	return job;
}

int put_job_in_kvs (job_t *job)
{
	if (job->kvs_dir == NULL)
		return -1;

	if (!kvsdir_exists (job->kvs_dir, "user"))
		kvsdir_put_string (job->kvs_dir, "user", job->user);
	if (!kvsdir_exists (job->kvs_dir, "jobname"))
		kvsdir_put_string (job->kvs_dir, "jobname", job->jobname);
	if (!kvsdir_exists (job->kvs_dir, "account"))
		kvsdir_put_string (job->kvs_dir, "account", job->account);
	if (!kvsdir_exists (job->kvs_dir, "submit_time"))
		kvsdir_put_double (job->kvs_dir, "submit_time", job->submit_time);
	if (!kvsdir_exists (job->kvs_dir, "execution_time"))
		kvsdir_put_double (job->kvs_dir, "execution_time", job->execution_time);
	if (!kvsdir_exists (job->kvs_dir, "time_limit"))
		kvsdir_put_double (job->kvs_dir, "time_limit", job->time_limit);
	if (!kvsdir_exists (job->kvs_dir, "nnodes"))
		kvsdir_put_int (job->kvs_dir, "nnodes", job->nnodes);
	if (!kvsdir_exists (job->kvs_dir, "ncpus"))
		kvsdir_put_int (job->kvs_dir, "ncpus", job->ncpus);
	if (!kvsdir_exists (job->kvs_dir, "io_size"))
		kvsdir_put_double (job->kvs_dir, "io_size", job->io_size);
	if (!kvsdir_exists (job->kvs_dir, "io_freq"))
		kvsdir_put_double (job->kvs_dir, "io_freq", job->io_freq);

	flux_t h = kvsdir_handle (job->kvs_dir);
	kvs_commit (h);

	//TODO: Check to see if this is necessary, i assume the kvsdir becomes stale after a commit
	char *dir_key;
	asprintf (&dir_key, kvsdir_key (job->kvs_dir));
	kvsdir_destroy (job->kvs_dir);
	kvs_get_dir (h, &job->kvs_dir, dir_key);
	free (dir_key);

	return 0;
}

job_t *pull_job_from_kvs (kvsdir_t kvsdir)
{
	job_t *job = blank_job();

	job->kvs_dir = kvsdir;

	sscanf (kvsdir_key (job->kvs_dir), "lwj.%d", &job->id);
	kvsdir_get_string (job->kvs_dir, "user", &job->user);
	kvsdir_get_string (job->kvs_dir, "jobname", &job->jobname);
	kvsdir_get_string (job->kvs_dir, "account", &job->account);
	kvsdir_get_double (job->kvs_dir, "submit_time", &job->submit_time);
	kvsdir_get_double (job->kvs_dir, "starting_time", &job->start_time);
	kvsdir_get_double (job->kvs_dir, "execution_time", &job->execution_time);
	kvsdir_get_double (job->kvs_dir, "io_time", &job->io_time);
	kvsdir_get_double (job->kvs_dir, "time_limit", &job->time_limit);
	kvsdir_get_int (job->kvs_dir, "nnodes", &job->nnodes);
	kvsdir_get_int (job->kvs_dir, "ncpus", &job->ncpus);
	kvsdir_get_double (job->kvs_dir, "io_size", &job->io_size);
	kvsdir_get_double (job->kvs_dir, "io_freq", &job->io_freq);

	return job;
}

int send_alive_request (flux_t h, const char* module_name)
{
	JSON o = Jnew ();
	Jadd_str (o, "mod_name", module_name);
	Jadd_int (o, "rank", flux_rank (h));
	if (flux_request_send (h, o, "%s", "sim.alive") < 0){
		Jput (o);
		return -1;
	}
	Jput (o);
	return 0;
}
