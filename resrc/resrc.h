#ifndef FLUX_RESRC_H
#define FLUX_RESRC_H

/*
 *  C API interface to Flux Resources
 */

#include <uuid/uuid.h>
#include "src/common/libutil/shortjson.h"

typedef struct resource_list resource_list_t;
typedef struct resources resources_t;
typedef struct resrc resrc_t;
typedef struct resrc_tree resrc_tree_t;

typedef enum {
    RESOURCE_INVALID,
    RESOURCE_IDLE,
    RESOURCE_ALLOCATED,
    RESOURCE_RESERVED,
    RESOURCE_DOWN,
    RESOURCE_UNKNOWN,
    RESOURCE_END
} resource_state_t;


/*
 * Return the type of the resouce
 */
char *resrc_type (resrc_t *resrc);

/*
 * Return the name of the resouce
 */
char *resrc_name (resrc_t *resrc);

/*
 * Return the physical tree for the resouce
 */
resrc_tree_t *resrc_phys_tree (resrc_t *resrc);

/*
 * Free memory allocated to a job id
 */
void jobid_destroy (void *object);

/*
 * Create a list of resource keys
 */
resource_list_t *resrc_new_id_list ();

/*
 * Destroy a list of resource keys
 */
void resrc_id_list_destroy (resource_list_t *resrc_ids);

/*
 * Get the first element in the result list
 */
char *resrc_list_first (resource_list_t *rl);

/*
 * Get the next element in the resource id list
 */
char *resrc_list_next ();

/*
 * Get the next element in the resource id list
 */
size_t resrc_list_size ();

/*
 * Create a new resource object
 */
resrc_t *resrc_new_resource (const char *type, const char *name, int64_t id,
                                uuid_t uuid);

/*
 * Create a copy of a resource object
 */
resrc_t *resrc_copy_resource (resrc_t *resrc);

/*
 * Destroy a resource object
 */
void resrc_resource_destroy (void *object);

/*
 * Create a hash table of all resources described by a configuration
 * file
 */
resources_t *resrc_generate_resources (const char *path, char*resource);

/*
 * De-allocate the resources handle
 */
void resrc_destroy_resources (resources_t **resources);

/*
 * Print details of a specific resource
 */
void resrc_print_resource (resrc_t *resrc);

/*
 * Provide a listing to stdout of every resource in hash table
 */
void resrc_print_resources (resources_t *resrcs);

/*
 * Determine whether a specific resource meets the input requirements
 * (basically whether the type matches right now...)
 * Inputs:  resrc     - the resource of question
 *          type      - the type to match
 *          available - when true, consider only idle resources
 *                      otherwise find all possible resources matching type
 * Returns: true if the resource meets the criteria
 */
bool resrc_find_resource (resrc_t *resrc, const char *type, bool available);

/*
 * Search a table of resources for the requested type
 * Inputs:  resrcs    - hash table of all resources
 *          found     - running list of keys to previously found resources
 *          req_res   - requested resource
 *          available - when true, consider only idle resources
 *                      otherwise find all possible resources matching type
 * Returns: the number of matching resources found
 *          found     - any resources found are added to this list
 *
 */
int resrc_search_flat_resources (resources_t *resrcs, resource_list_t *found,
                                 JSON req_res, bool available);

/*
 * Allocate a set of resources to a job
 */
int resrc_allocate_resources (resources_t *resrcs, resource_list_t *resrc_ids,
                              int64_t job_id);

/*
 * Reserve a set of resources to a job
 */
int resrc_reserve_resources (resources_t *resrcs, resource_list_t *resrc_ids,
                             int64_t job_id);

/*
 * Create a json object containing the resources present in the input
 * list
 */
json_object *resrc_serialize (resources_t *resrcs, resource_list_t *resrc_ids);

/*
 * Remove a job allocation from a set of resources
 */
int resrc_release_resources (resources_t *resrcs, resource_list_t *resrc_ids,
                             int64_t rel_job);


#endif /* !FLUX_RESRC_H */
