#ifndef FLUX_RESRC_TREE_H
#define FLUX_RESRC_TREE_H

/*
 *  C API interface to Flux Resource Tree
 */

#include "resrc.h"

typedef struct resrc_tree_list *resrc_tree_list_t;
typedef struct resrc_reqst *resrc_reqst_t;
typedef struct resrc_reqst_list *resrc_reqst_list_t;

/***********************************************************************
 * Resource tree API
 ***********************************************************************/
/*
 * Return the resrc_t associated with this tree
 */
resrc_t resrc_tree_resrc (resrc_tree_t resrc_tree);

/*
 * Return the number of children in the resource tree
 */
size_t resrc_tree_num_children (resrc_tree_t resrc_tree);

/*
 * Return the list of child resource trees for the resouce tree input
 */
resrc_tree_list_t resrc_tree_children (resrc_tree_t resrc_tree);

/*
 * Add a child resource tree to the input resource tree
 */
int resrc_tree_add_child (resrc_tree_t parent, resrc_tree_t child);

/*
 * Create a new resrc_tree_t object
 */
resrc_tree_t resrc_tree_new (resrc_tree_t parent, resrc_t resrc);

/*
 * Return a copy of the input resource tree
 */
resrc_tree_t resrc_tree_copy (resrc_tree_t resrc_tree);

/*
 * Free a resrc_tree_t object
 */
void resrc_tree_free (resrc_tree_t resrc_tree);

/*
 * Destroy an entire tree of resrc_tree_t objects
 */
void resrc_tree_destroy (resrc_tree_t resrc_tree);

/*
 * Print the resources in a resrc_tree_t object
 */
void resrc_tree_print (resrc_tree_t resrc_tree);

/*
 * Add the input resource tree to the json object
 */
int resrc_tree_serialize (JSON o, resrc_tree_t rt);

/*
 * Allocate all the resources in a resource tree
 */
int resrc_tree_allocate (resrc_tree_t rt, int64_t job_id);

/*
 * Reserve all the resources in a resource tree
 */
int resrc_tree_reserve (resrc_tree_t rt, int64_t job_id);

/*
 * Release all the resources in a resource tree
 */
int resrc_tree_release (resrc_tree_t rt, int64_t job_id);

/*
 * Search a list of resource trees for a specific, composite resource
 * Inputs:  resrc_trees - the list of resource trees to search
 *          found       - running list of keys to previously found resources
 *          sample_tree - the resource tree to search for
 *          available   - when true, consider only idle resources
 *                        otherwise find all possible resources matching type
 * Returns: the number of matching resource composites found
 *          found       - any resources found are added to this list
 */
int resrc_tree_search (resrc_tree_list_t resrc_trees,
                       resrc_reqst_t sample_tree,
                       resrc_tree_list_t found_trees, bool available);

/***********************************************************************
 * Resource tree list
 ***********************************************************************/
/*
 * Create a new list of resrc_tree_t objects
 */
resrc_tree_list_t resrc_tree_new_list ();

/*
 * Append a resource tree to the resource tree list
 */
int resrc_tree_list_append (resrc_tree_list_t rtl, resrc_tree_t rt);

/*
 * Get the first element in the resource tree list
 */
resrc_tree_t resrc_tree_list_first (resrc_tree_list_t rtl);

/*
 * Get the next element in the resource tree list
 */
resrc_tree_t resrc_tree_list_next (resrc_tree_list_t rtl);

/*
 * Get the number of elements in the resource tree list
 */
size_t resrc_tree_list_size (resrc_tree_list_t rtl);

/*
 * Destroy a resrc_tree_list_t object
 */
void resrc_tree_list_destroy (resrc_tree_list_t rtl);

/*
 * Add the input list of resource trees to the json object
 */
int resrc_tree_list_serialize (JSON o, resrc_tree_list_t rtl);

/*
 * Allocate all the resources in a list of resource trees
 */
int resrc_tree_list_allocate (resrc_tree_list_t rtl, int64_t job_id);

/*
 * Reserve all the resources in a list of resource trees
 */
int resrc_tree_list_reserve (resrc_tree_list_t rtl, int64_t job_id);

/*
 * Release all the resources in a list of resource trees
 */
int resrc_tree_list_release (resrc_tree_list_t rtl, int64_t job_id);


/***********************************************************************
 * Composite resource request
 ***********************************************************************/

/*
 * Return the resrc_t associated with this request
 */
resrc_t resrc_reqst_resrc (resrc_reqst_t resrc_reqst);

/*
 * Return the required number of resources in this request
 */
int64_t resrc_reqst_reqrd (resrc_reqst_t resrc_reqst);

/*
 * Return the number of resources found for this request
 */
int64_t resrc_reqst_nfound (resrc_reqst_t resrc_reqst);

/*
 * Increment the number of resources found for this request
 */
int64_t resrc_reqst_add_found (resrc_reqst_t resrc_reqst, int64_t nfound);

/*
 * Clear the number of resources found for this request
 */
void resrc_reqst_clear_found (resrc_reqst_t resrc_reqst);

/*
 * Return the number of children in the resource request
 */
size_t resrc_reqst_num_children (resrc_reqst_t resrc_reqst);

/*
 * Return the list of child resource requests for the resouce request input
 */
resrc_reqst_list_t resrc_reqst_children (resrc_reqst_t resrc_reqst);

/*
 * Add a child resource request to the input resource request
 */
int resrc_reqst_add_child (resrc_reqst_t parent, resrc_reqst_t child);

/*
 * Create a new resrc_reqst_t object
 */
resrc_reqst_t resrc_reqst_new (resrc_t resrc, int64_t qty);

/*
 * Create a resrc_reqst_t object from a json object
 */
resrc_reqst_t resrc_reqst_from_json (JSON o, resrc_t parent);

/*
 * Print the resources in a resrc_reqst_t object
 */
void resrc_reqst_print (resrc_reqst_t resrc_reqst);

/*
 * Destroy a resrc_reqst_t object
 */
void resrc_reqst_destroy (resrc_reqst_t resrc_reqst);

/***********************************************************************
 * Resource request list
 ***********************************************************************/
/*
 * Create a new list of resrc_reqst_t objects
 */
resrc_reqst_list_t resrc_reqst_new_list ();

/*
 * Append a resource reqst to the resource reqst list
 */
int resrc_reqst_list_append (resrc_reqst_list_t rrl, resrc_reqst_t rr);

/*
 * Get the first element in the resource reqst list
 */
resrc_reqst_t resrc_reqst_list_first (resrc_reqst_list_t rrl);

/*
 * Get the next element in the resource reqst list
 */
resrc_reqst_t resrc_reqst_list_next (resrc_reqst_list_t rrl);

/*
 * Get the number of elements in the resource reqst list
 */
size_t resrc_reqst_list_size (resrc_reqst_list_t rrl);

/*
 * Destroy a resrc_reqst_list_t object
 */
void resrc_reqst_list_destroy (resrc_reqst_list_t rrl);



#endif /* !FLUX_RESRC_TREE_H */
