#ifndef SIMULATOR_H
#define SIMULATOR_H 1

#include <czmq.h>
#include <flux/core.h>
#include "src/common/libutil/shortjson.h"

typedef struct {
  double sim_time;
  zhash_t *timers;
} sim_state_t;

typedef struct {
	int id;
	char* user;
	char* jobname;
	char* account;
	double submit_time;
	double start_time;
	double execution_time;
	double io_time;
	double time_limit;
	int nnodes;
	int ncpus;
	int64_t io_rate;
	kvsdir_t *kvs_dir;
} job_t;

sim_state_t *new_simstate ();
void free_simstate (sim_state_t* sim_state);
JSON sim_state_to_json(sim_state_t *state);
sim_state_t *json_to_sim_state(JSON o);
int print_values (const char *key, void *item, void *argument);

int put_job_in_kvs (job_t *job);
job_t *pull_job_from_kvs (kvsdir_t *kvs_dir);
void free_job (job_t *job);
job_t *blank_job ();
int send_alive_request (flux_t h, const char* module_name);
int send_reply_request (flux_t h, sim_state_t *sim_state, const char *module_name);

zhash_t *zhash_fromargv (int argc, char **argv);

/*
struct rdl *get_rdl (flux_t h, char *path);
void close_rdl ();
*/
#endif /* SIMULATOR_H */
