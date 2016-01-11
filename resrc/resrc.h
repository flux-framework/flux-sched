#ifndef FLUX_RESRC_H
#define FLUX_RESRC_H

/*
 *  C API interface to Flux Resources
 */

#include <uuid/uuid.h>
#include "src/common/libutil/shortjson.h"

#define TIME_MAX INT64_MAX

typedef struct resrc resrc_t;
typedef struct resrc_reqst resrc_reqst_t;
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
 * Return the fully qualified name of the resouce
 */
char *resrc_path (resrc_t *resrc);

/*
 * Return the name of the resouce
 */
char *resrc_name (resrc_t *resrc);

/*
 * Return the digest of resrc -- key to find corresponding
 * broker rank
 */
char *resrc_digest (resrc_t *resrc);
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
 * Return the amount of the resource available at the given time
 */
size_t resrc_available_at_time (resrc_t *resrc, int64_t time);

/*
 * Return the least amount of the resource available during the time
 * range
 */
size_t resrc_available_during_range (resrc_t *resrc, int64_t range_starttime,
                                     int64_t range_endtime, bool exclusive);

/*
 * Return the resource state as a string
 */
char* resrc_state (resrc_t *resrc);

/*
 * Return the physical tree for the resouce
 */
resrc_tree_t *resrc_phys_tree (resrc_t *resrc);

/*
 * Create a new resource object
 */
resrc_t *resrc_new_resource (const char *type, const char *path,
                             const char *name, int64_t id, uuid_t uuid,
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
resrc_t *resrc_new_from_json (JSON o, resrc_t *parent, bool physical);

/*
 * Return the head of a resource tree of all resources described by an
 * rdl-formatted configuration file
 */
resrc_t *resrc_generate_rdl_resources (const char *path, char*resource);

/*
 * Return the head of a resource tree of all resources described by an
 * xml serialization
 */
resrc_t *resrc_generate_xml_resources (resrc_t *host_resrc, const char *buf,
                                       size_t length);

/*
 * Add the input resource to the json object
 */
int resrc_to_json (JSON o, resrc_t *resrc);

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
 * Stage size elements of a resource
 */
void resrc_stage_resrc(resrc_t *resrc, size_t size);

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
