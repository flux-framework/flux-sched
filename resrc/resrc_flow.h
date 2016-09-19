#ifndef FLUX_RESRC_FLOW_H
#define FLUX_RESRC_FLOW_H

/*
 *  C API interface to Flux Resource Flow Graph
 */

#include "resrc.h"

typedef struct resrc_flow_list resrc_flow_list_t;

/***********************************************************************
 * Resource flow API
 ***********************************************************************/
/*
 * Return the resrc_t associated with this flow
 */
resrc_t *resrc_flow_resrc (resrc_flow_t *resrc_flow);

/*
 * Return the resrc_t containing the flow information
 */
resrc_t *resrc_flow_flow_resrc (resrc_flow_t *resrc_flow);

/*
 * Return the number of children in the resource flow
 */
size_t resrc_flow_num_children (resrc_flow_t *resrc_flow);

/*
 * Return the list of child resource flows for the resouce flow input
 */
resrc_flow_list_t *resrc_flow_children (resrc_flow_t *resrc_flow);

/*
 * Add a child resource flow to the input resource flow
 */
int resrc_flow_add_child (resrc_flow_t *parent, resrc_flow_t *child);

/*
 * Create a new resrc_flow_t object
 */
resrc_flow_t *resrc_flow_new (resrc_flow_t *parent, resrc_t *flow_resrc,
                              resrc_t *resrc);

/*
 * Return a copy of the input resource flow
 */
resrc_flow_t *resrc_flow_copy (resrc_flow_t *resrc_flow);

/*
 * Destroy an entire flow of resrc_flow_t objects
 */
void resrc_flow_destroy (resrc_flow_t *resrc_flow);

/*
 * Create a resrc_flow_t object from a json object
 */
resrc_flow_t *resrc_flow_new_from_json (JSON o, resrc_flow_t *parent);

/*
 * Return the head of a resource flow tree of all resources described
 * by an rdl-formatted configuration file
 */
resrc_flow_t *resrc_flow_generate_rdl (const char *path, char *uri);

/*
 * Print the resources in a resrc_flow_t object
 */
void resrc_flow_print (resrc_flow_t *resrc_flow);

/*
 * Add the input resource flow to the json object
 */
int resrc_flow_serialize (json_object *o, resrc_flow_t *resrc_flow);

/*
 * Allocate all the resources in a resource flow
 */
int resrc_flow_allocate (resrc_flow_t *resrc_flow, int64_t job_id,
                         int64_t starttime, int64_t endtime);

/*
 * Reserve all the resources in a resource flow
 */
int resrc_flow_reserve (resrc_flow_t *resrc_flow, int64_t job_id,
                        int64_t starttime, int64_t endtime);

/*
 * Release an allocation from a resource flow
 */
int resrc_flow_release (resrc_flow_t *resrc_flow, int64_t job_id);

/*
 * Remove all reservations from a resource flow
 */
int resrc_flow_release_all_reservations (resrc_flow_t *resrc_flow);

/*
 * Stage the required quantity of the resource flow
 */
int resrc_flow_stage_resources (resrc_flow_t *resrc_flow, size_t size);

/*
 * Unstage all resources in a resource flow
 */
int resrc_flow_unstage_resources (resrc_flow_t *resrc_flow);

/*
 * Create a resource flow from a json object
 */
resrc_flow_t *resrc_flow_deserialize (json_object *o, resrc_flow_t *parent);

/*
 * We rely on the available value of this node in the flow graph being
 * current.  This eliminates the need to check every parent up the
 * tree for available flow.
 */
bool resrc_flow_available (resrc_flow_t *resrc_flow, size_t flow,
                           resrc_reqst_t *request);

/***********************************************************************
 * Resource flow list
 ***********************************************************************/
/*
 * Create a new list of resrc_flow_t objects
 */
resrc_flow_list_t *resrc_flow_list_new ();

/*
 * Append a resource flow to the resource flow list
 */
int resrc_flow_list_append (resrc_flow_list_t *rfl, resrc_flow_t *rf);

/*
 * Get the first element in the resource flow list
 */
resrc_flow_t *resrc_flow_list_first (resrc_flow_list_t *rfl);

/*
 * Get the next element in the resource flow list
 */
resrc_flow_t *resrc_flow_list_next (resrc_flow_list_t *rfl);

/*
 * Get the number of elements in the resource flow list
 */
size_t resrc_flow_list_size (resrc_flow_list_t *rfl);

/*
 * Remove an item from the resource flow list
 */
void resrc_flow_list_remove (resrc_flow_list_t *rfl, resrc_flow_t *rf);

/*
 * Destroy a resrc_flow_list_t object including all children
 */
void resrc_flow_list_destroy (resrc_flow_list_t *rfl);

/*
 * Add the input list of resource flows to the json array object
 */
int resrc_flow_list_serialize (json_object *o, resrc_flow_list_t *rfl);

/*
 * Create a resource flow list from a json object
 */
resrc_flow_list_t *resrc_flow_list_deserialize (json_object *o);

/*
 * Allocate all the resources in a list of resource flows
 */
int resrc_flow_list_allocate (resrc_flow_list_t *rtl, int64_t job_id,
                              int64_t starttime, int64_t endtime);

/*
 * Reserve all the resources in a list of resource flows
 */
int resrc_flow_list_reserve (resrc_flow_list_t *rtl, int64_t job_id,
                             int64_t starttime, int64_t endtime);

/*
 * Release an allocation from a list of resource flows
 */
int resrc_flow_list_release (resrc_flow_list_t *rtl, int64_t job_id);

/*
 * Release all the reservations from a list of resource flows
 */
int resrc_flow_list_release_all_reservations (resrc_flow_list_t *rtl);

/*
 * Unstage all resources in a list of resource flows
 */
void resrc_flow_list_unstage_resources (resrc_flow_list_t *rtl);


#endif /* !FLUX_RESRC_FLOW_H */
