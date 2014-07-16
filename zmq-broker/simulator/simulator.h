#ifndef SIMULATOR_H
#define SIMULATOR_H 1

#include <czmq.h>
#include "shortjson.h"

typedef struct {
  int sim_time;
  zhash_t *timers;
} sim_state_t;


sim_state_t *new_simstate ();
void free_simstate (sim_state_t* sim_state);
JSON sim_state_to_json(sim_state_t *state);
sim_state_t *json_to_sim_state(JSON o);
int print_values (const char *key, void *item, void *argument);

#endif /* SIMULATOR_H */
