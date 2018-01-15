#ifndef FLUX_RESRC_TREE_H
#define FLUX_RESRC_TREE_H

/*
 *  C API interface to Flux Resource Tree
 */

#include "resrc.h"

typedef struct resrc_tree_list resrc_tree_list_t;

/***********************************************************************
 * Resource tree API
 ***********************************************************************/
/*
 * Return the resrc_t associated with this tree
 */
resrc_t *resrc_tree_resrc (resrc_tree_t *resrc_tree);

/*
 * Return the root node of this tree
 */
resrc_tree_t *resrc_tree_root (resrc_api_ctx_t *ctx);

/*
 * Return the name of this tree
 */
const char *resrc_tree_name (resrc_api_ctx_t *ctx);

/*
 * Return the number of children in the resource tree
 */
size_t resrc_tree_num_children (resrc_tree_t *resrc_tree);

/*
 * Return the list of child resource trees for the resouce tree input
 */
resrc_tree_list_t *resrc_tree_children (resrc_tree_t *resrc_tree);

/*
 * Add a child resource tree to the input resource tree
 */
int resrc_tree_add_child (resrc_tree_t *parent, resrc_tree_t *child);

/*
 * Create a new resrc_tree_t object
 */
resrc_tree_t *resrc_tree_new (resrc_tree_t *parent, resrc_t *resrc);

/*
 * Return a copy of the input resource tree. This copies the resource and all 
 * its children but not any deeper. A semi-shallow copy.
 */
resrc_tree_t *resrc_tree_copy (resrc_tree_t *resrc_tree);

/*
 * Return a copy of the input resource tree, also copying its children,
 * its children's children, etc.
 */
resrc_tree_t *resrc_tree_deep_copy (resrc_tree_t *resrc_tree);

/*
 * Destroy an entire tree of resrc_tree_t objects
 */
void resrc_tree_destroy (resrc_api_ctx_t *ctx, resrc_tree_t *resrc_tree,
                         bool is_root, bool destroy_resrc);

/*
 * Print the resources in a resrc_tree_t object
 */
void resrc_tree_print (resrc_tree_t *resrc_tree);

/*
 * Add the input resource tree to the json object
 */
int resrc_tree_serialize (json_t *o, resrc_tree_t *resrc_tree);

/*
 * Create a resource tree from a json object
 */
resrc_tree_t *resrc_tree_deserialize (resrc_api_ctx_t *ctx,
                                      json_t *o, resrc_tree_t *parent);

/*
 * Allocate all the resources in a resource tree
 */
int resrc_tree_allocate (resrc_tree_t *resrc_tree, int64_t job_id,
                         int64_t starttime, int64_t endtime);

/*
 * Allocate all the resources in a resource tree and print node list to file
 */
int resrc_tree_allocate_print (resrc_tree_t *resrc_tree, int64_t job_id,
                               int64_t starttime, int64_t endtime, FILE *alloc_file);
/*
 * Reserve all the resources in a resource tree
 */
int resrc_tree_reserve (resrc_tree_t *resrc_tree, int64_t job_id,
                        int64_t starttime, int64_t endtime);

/*
 * Release an allocation from a resource tree
 */
int resrc_tree_release (resrc_tree_t *resrc_tree, int64_t job_id);

/*
 * Remove all reservations from a resource tree
 */
int resrc_tree_release_all_reservations (resrc_tree_t *resrc_tree);

/*
 * Unstage all resources in a resource tree
 */
void resrc_tree_unstage_resources (resrc_tree_t *resrc_tree);

/***********************************************************************
 * Resource tree list
 ***********************************************************************/
/*
 * Create a new list of resrc_tree_t objects
 */
resrc_tree_list_t *resrc_tree_list_new ();

/*
 * Append a resource tree to the resource tree list
 */
int resrc_tree_list_append (resrc_tree_list_t *rtl, resrc_tree_t *rt);

/*
 * Get the first element in the resource tree list
 */
resrc_tree_t *resrc_tree_list_first (resrc_tree_list_t *rtl);

/*
 * Get the next element in the resource tree list
 */
resrc_tree_t *resrc_tree_list_next (resrc_tree_list_t *rtl);

/*
 * Get the number of elements in the resource tree list
 */
size_t resrc_tree_list_size (resrc_tree_list_t *rtl);

/*
 * Remove an item from the resource tree list
 */
void resrc_tree_list_remove (resrc_tree_list_t *rtl, resrc_tree_t *rt);

/*
 * Destroy a resrc_tree_list_t object including all children
 */
void resrc_tree_list_destroy (resrc_api_ctx_t *ctx, resrc_tree_list_t *rtl,
                              bool destroy_resrc);

/*
 * Desroy just the resrc_tree_list and not their children
 */
void resrc_tree_list_shallow_destroy (resrc_tree_list_t *rtl);

/*
 * Add the input list of resource trees to the json array object
 */
int resrc_tree_list_serialize (json_t *o, resrc_tree_list_t *rtl);

/*
 * Create a resource tree list from a json object
 */
resrc_tree_list_t *resrc_tree_list_deserialize (resrc_api_ctx_t *ctx, json_t *o);

/*
 * Allocate all the resources in a list of resource trees
 */
int resrc_tree_list_allocate (resrc_tree_list_t *rtl, int64_t job_id,
                              int64_t starttime, int64_t endtime);
/*
 * Allocate all the resources in a list of resource trees and print to a file
 */
int resrc_tree_list_allocate_print (resrc_tree_list_t *rtl, int64_t job_id,
                                    int64_t starttime, int64_t endtime, FILE *alloc_file);

/*
 * Reserve all the resources in a list of resource trees
 */
int resrc_tree_list_reserve (resrc_tree_list_t *rtl, int64_t job_id,
                             int64_t starttime, int64_t endtime);

/*
 * Release an allocation from a list of resource trees
 */
int resrc_tree_list_release (resrc_tree_list_t *rtl, int64_t job_id);

/*
 * Release all the reservations from a list of resource trees
 */
int resrc_tree_list_release_all_reservations (resrc_tree_list_t *rtl);

/*
 * Unstage all resources in a list of resource trees
 */
void resrc_tree_list_unstage_resources (resrc_tree_list_t *rtl);


#endif /* !FLUX_RESRC_TREE_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
