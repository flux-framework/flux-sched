#ifndef FLUX_RESRC_REQST_H
#define FLUX_RESRC_REQST_H

/*
 *  C API interface to Flux Resource Request
 */

#include "resrc.h"
#include "resrc_tree.h"


typedef struct resrc_reqst_list resrc_reqst_list_t;

/***********************************************************************
 * Composite resource request
 ***********************************************************************/

/*
 * Return the resrc_t associated with this request
 */
resrc_t *resrc_reqst_resrc (resrc_reqst_t *resrc_reqst);

/*
 * Return the start time of this request
 */
int64_t resrc_reqst_starttime (resrc_reqst_t *resrc_reqst);

/*
 * Set the start time of this request
 */
int resrc_reqst_set_starttime (resrc_reqst_t *resrc_reqst, int64_t time);

/*
 * Return the end time of this request
 */
int64_t resrc_reqst_endtime (resrc_reqst_t *resrc_reqst);

/*
 * Set the end time of this request
 */
int resrc_reqst_set_endtime (resrc_reqst_t *resrc_reqst, int64_t time);

/*
 * Return whether the request is for an exclusive allocation
 */
bool resrc_reqst_exclusive (resrc_reqst_t *resrc_reqst);

/*
 * Return the required number of resources in this request
 */
int64_t resrc_reqst_reqrd_qty (resrc_reqst_t *resrc_reqst);

/*
 * Return the required size of resource in this request
 */
int64_t resrc_reqst_reqrd_size (resrc_reqst_t *resrc_reqst);

/*
 * Return the number of resources found for this request
 */
int64_t resrc_reqst_nfound (resrc_reqst_t *resrc_reqst);

/*
 * Increment the number of resources found for this request
 */
int64_t resrc_reqst_add_found (resrc_reqst_t *resrc_reqst, int64_t nfound);

/*
 * Clear the number of resources found for this request
 */
int resrc_reqst_clear_found (resrc_reqst_t *resrc_reqst);

/*
 * Return true if the required number of resources were found for the
 * resource request and all its children
 */
bool resrc_reqst_all_found (resrc_reqst_t *resrc_reqst);

/*
 * Return the number of children in the resource request
 */
size_t resrc_reqst_num_children (resrc_reqst_t *resrc_reqst);

/*
 * Return the list of child resource requests for the resouce request input
 */
resrc_reqst_list_t *resrc_reqst_children (resrc_reqst_t *resrc_reqst);

/*
 * Add a child resource request to the input resource request
 */
int resrc_reqst_add_child (resrc_reqst_t *parent, resrc_reqst_t *child);

/*
 * Create a new resrc_reqst_t object
 */
resrc_reqst_t *resrc_reqst_new (resrc_t *resrc, int64_t qty, int64_t size,
                                int64_t starttime, int64_t endtime,
                                bool exclusive);

/*
 * Create a resrc_reqst_t object from a json object
 */
resrc_reqst_t *resrc_reqst_from_json (json_object *o, resrc_reqst_t *parent);

/*
 * Destroy a resrc_reqst_t object
 */
void resrc_reqst_destroy (resrc_reqst_t *resrc_reqst);

/*
 * Print the resources in a resrc_reqst_t object
 */
void resrc_reqst_print (resrc_reqst_t *resrc_reqst);

/***********************************************************************
 * Resource request list
 ***********************************************************************/
/*
 * Create a new list of resrc_reqst_t objects
 */
resrc_reqst_list_t *resrc_reqst_list_new ();

/*
 * Append a resource reqst to the resource reqst list
 */
int resrc_reqst_list_append (resrc_reqst_list_t *rrl, resrc_reqst_t *rr);

/*
 * Get the first element in the resource reqst list
 */
resrc_reqst_t *resrc_reqst_list_first (resrc_reqst_list_t *rrl);

/*
 * Get the next element in the resource reqst list
 */
resrc_reqst_t *resrc_reqst_list_next (resrc_reqst_list_t *rrl);

/*
 * Get the number of elements in the resource reqst list
 */
size_t resrc_reqst_list_size (resrc_reqst_list_t *rrl);

/*
 * Remove an item from the resource reqst list
 */
void resrc_reqst_list_remove (resrc_reqst_list_t *rrl, resrc_reqst_t *rr);

/*
 * Destroy a resrc_reqst_list_t object
 */
void resrc_reqst_list_destroy (resrc_reqst_list_t *rrl);

/*
 * Search a list of resource trees for a specific, composite resource
 * Inputs:  resrc_in    - the resource at the head of the tree
 *          resrc_reqst - the requested resource or resource composite
 *          found_tree  - the tree that will contain found resources
 *          available   - when true, consider only idle resources
 *                        otherwise find all possible resources matching type
 * Returns: the number of matching resource composites found
 *          found_tree  - any resources found are added to this tree
 */
int64_t resrc_tree_search (resrc_t *resrc_in, resrc_reqst_t *resrc_reqst,
                           resrc_tree_t **found_tree, bool available);


#endif /* !FLUX_RESRC_REQST_H */
