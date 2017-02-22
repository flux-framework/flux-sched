#ifndef FLUX_RESRC_H
#define FLUX_RESRC_H

/*
 *  C API interface to Flux Resources
 */

#include <uuid/uuid.h>
#include "planner.h"

#define TIME_MAX INT64_MAX - 10

typedef struct hwloc_topology * TOPOLOGY;
typedef struct resrc resrc_t;
typedef struct resrc_reqst resrc_reqst_t;
typedef struct resrc_tree resrc_tree_t;
typedef struct resrc_flow resrc_flow_t;

typedef enum {
    RESOURCE_INVALID,
    RESOURCE_IDLE,
    RESOURCE_ALLOCATED,
    RESOURCE_RESERVED,
    RESOURCE_DOWN,
    RESOURCE_UNKNOWN,
    RESOURCE_END
} resource_state_t;

typedef struct resrc_graph_req {
    char   *name;
    size_t size;
} resrc_graph_req_t;

/*
 * Initialize necessary components of the resource library
 */
void resrc_init (void);

/*
 * Destroy all internal components of the resource library
 */
void resrc_fini (void);

/*
 * Return the type of the resouce
 */
char *resrc_type (resrc_t *resrc);

/*
 * Return the fully qualified name of the resouce
 */
char *resrc_path (resrc_t *resrc);

/*
 * Return the basename of the resouce
 */
char *resrc_basename (resrc_t *resrc);

/*
 * Return the name of the resouce
 */
char *resrc_name (resrc_t *resrc);

 /*
 * Return the digest of resrc -- key to find corresponding
 * broker rank
 */
char *resrc_digest (resrc_t *resrc);

 /*
 * Set the digest field of resrc with 'digest'. This will
 * return the old digest.
 */
char *resrc_set_digest (resrc_t *resrc, char *digest);

/*
 * Return the id of the resouce
 */
int64_t resrc_id (resrc_t *resrc);

/*
 * Return the size of the resource
 */
size_t resrc_size (resrc_t *resrc);

/*
 * Return the quantity of available units
 */
size_t resrc_available (resrc_t *resrc);

/*
 * Return 0 if the required amount of the resource is available at the given time;
 * otehr -1.
 */
int resrc_available_at_time (resrc_t *resrc, int64_t time, size_t reqrd_size);

/*
 * Return 0 if the required amount of the resource is available during the time
 * range
 */
int resrc_available_during_range (resrc_t *resrc, int64_t range_starttime,
        int64_t range_endtime, size_t reqrd_size, bool exclusive);

/*
 * Return the resource state as a string
 */
char* resrc_state (resrc_t *resrc);

/*
 * Return twindow of planner_t type
 */
planner_t *resrc_twindow (resrc_t *resrc);

/*
 * Return the physical tree for the resouce
 */
resrc_tree_t *resrc_phys_tree (resrc_t *resrc);

/*
 * Return the number of jobs allocated to this resource
 */
size_t resrc_size_allocs (resrc_t *resrc);

/*
 * Return the number of jobs reserved for this resource
 */
size_t resrc_size_reservtns (resrc_t *resrc);

/*
 *  Insert item into twindow hash table with specified key and item.
 *  If key is already present returns -1 and leaves existing item
 *  unchanged.  Returns 0 on success.
 */
int resrc_twindow_insert (resrc_t *resrc, const char *key, int64_t starttime, int64_t endtime);

/*
 *  Insert a resource flow pointer into the graph table using the
 *  specified name.  If key is already present returns -1 and leaves
 *  existing item unchanged.  Returns 0 on success.
 */
int resrc_graph_insert (resrc_t *resrc, const char *name, resrc_flow_t *flow);

/*
 * Return the pointer to the resource with the given path
 */
resrc_t *resrc_lookup (const char *path);

/*
 * Create a new resource object
 */
resrc_t *resrc_new_resource (const char *type, const char *path,
                             const char *basename, const char *name,
                             const char *sig, int64_t id, uuid_t uuid,
                             size_t size);

/*
 * Create a copy of a resource object
 */
resrc_t *resrc_copy_resource (resrc_t *resrc);

/*
 * Destroy a resource object
 */
void resrc_resource_destroy (void *object);

/*
 * Create a resrc_t object from a json object
 */
resrc_t *resrc_new_from_json (json_t *o, resrc_t *parent, bool physical);

/*
 * Return the head of a resource tree of all resources described by an
 * rdl-formatted configuration file
 */
resrc_t *resrc_generate_rdl_resources (const char *path, char*resource);

/*
 * Return the head of a resource tree of all resources described by a
 * hwloc topology or NULL if errors are encountered.
 * Note: If err_str is non-null and errors are encountered, err_str will
 *       contain reason why.  Caller must subsequently free err_str.
 */
resrc_t *resrc_generate_hwloc_resources (resrc_t *host_resrc, TOPOLOGY topo,
                                         const char *sig, char **err_str);

/*
 * Add the input resource to the json object
 */
int resrc_to_json (json_t *o, resrc_t *resrc);

/*
 * Print details of a specific resource to a string buffer
 * and return to the caller. The caller must free it.
 */
char *resrc_to_string (resrc_t *resrc);
/*
 * Print details of a specific resource to stdout
 */
void resrc_print_resource (resrc_t *resrc);

/*
 * Convenience function to create a specialized cluster resource
 */
resrc_t *resrc_create_cluster (char *cluster);

/*
 * Finds if a resource request matches the specified resource over a period
 * defined by the start and end times.
 */
bool resrc_walltime_match (resrc_t *resrc, resrc_reqst_t *request,
                           size_t reqrd_size);

/*
 * Determine whether a specific resource has the required characteristics
 * Inputs:  resrc     - the specific resource under evaluation
 *          request   - resource request with the required characteristics
 *          available - when true, consider only idle resources
 *                      otherwise find all possible resources matching type
 * Returns: true if the input resource has the required characteristics
 */
bool resrc_match_resource (resrc_t *resrc, resrc_reqst_t *request,
                           bool available);

/*
 * Stage size elements of a resource along with any associated graph
 * resources
 */
int resrc_stage_resrc (resrc_t *resrc, size_t size,
                       resrc_graph_req_t *graph_req);

/*
 * Zero-out all the staged elements of a resource
 */
int resrc_unstage_resrc (resrc_t *resrc);

/*
 * Allocate a resource to a job
 */
int resrc_allocate_resource (resrc_t *resrc, int64_t job_id,
                             int64_t starttime, int64_t endtime);

/*
 * Reserve a resource for a job
 */
int resrc_reserve_resource (resrc_t *resrc, int64_t job_id,
                            int64_t starttime, int64_t endtime);

/*
 * Remove a job allocation from a resource
 * Supports both now and time-based allocations.
 */
int resrc_release_allocation (resrc_t *resrc, int64_t rel_job);

/*
 * Remove all reservations of a resource
 * Supports both now and time-based reservations.
 */
int resrc_release_all_reservations (resrc_t *resrc);

/*
 * Get epoch time
 */
static inline int64_t epochtime ()
{
    return (int64_t) time (NULL);
}


#endif /* !FLUX_RESRC_H */
