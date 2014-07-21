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
			//zhash_destroy (& (sim_state->timers));
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
