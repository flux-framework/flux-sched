#ifndef FLUX_RESRC_TREE_H
#define FLUX_RESRC_TREE_H

/*
 *  C API interface to Flux Resource Tree
 */

#include "resrc.h"

typedef struct resrc_tree_list *resrc_tree_list_t;

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


#endif /* !FLUX_RESRC_TREE_H */
