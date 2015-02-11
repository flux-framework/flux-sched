#ifndef FLUX_RESRC_TREE_H
#define FLUX_RESRC_TREE_H

/*
 *  C API interface to Flux Resource Tree
 */

#include "resrc.h"


/*
 * Return the list of child resource trees for the resouce tree input
 */
resource_list_t *resrc_tree_children (resrc_tree_t *resrc_tree);

/*
 * Create a new resrc_tree_t object
 */
resrc_tree_t *resrc_tree_new (resrc_tree_t *parent, resrc_t *resrc);

/*
 * Add a child resource tree to the input resource tree
 */
int resrc_tree_add_child (resrc_tree_t *tree, resrc_tree_t *child);

/*
 * Return a copy of the input resource tree
 */
resrc_tree_t* resrc_tree_copy (resrc_tree_t *resrc_tree);

/*
 * Destroy a resrc_tree_t object
 */
void resrc_tree_destroy (resrc_tree_t *resrc_tree);

/*
 * Print the resources in a resrc_tree_t object
 */
void resrc_tree_print (resrc_tree_t *resrc_tree);

/*
 * Search a list of resource trees for a specific, composite resource
 * Inputs:  resrcs    - the list of resources to search
 *          found     - running list of keys to previously found resources
 *          req_res   - requested composite resource
 *          available - when true, consider only idle resources
 *                      otherwise find all possible resources matching type
 * Returns: the number of matching resource composites found
 *          found     - any resources found are added to this list
 */
int resrc_tree_search (resource_list_t *resrcs, resource_list_t *found,
                       JSON req_res, bool available);


#endif /* !FLUX_RESRC_TREE_H */
