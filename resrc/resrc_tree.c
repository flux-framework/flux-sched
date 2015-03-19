/*****************************************************************************\
 *  Copyright (c) 2014 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
 *  LLNL-CODE-658032 All rights reserved.
 *
 *  This file is part of the Flux resource manager framework.
 *  For details, see https://github.com/flux-framework.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license, or (at your option)
 *  any later version.
 *
 *  Flux is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *  See also:  http://www.gnu.org/licenses/
\*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <czmq.h>

#include "rdl.h"
#include "resrc_tree.h"
#include "src/common/libutil/xzmalloc.h"

typedef struct zlist_t resrc_tree_list;

struct resrc_tree {
    resrc_tree_t *parent;
    resrc_t *resrc;
    zlist_t *children;
};


/***************************************************************************
 *  API
 ***************************************************************************/

resrc_t *resrc_tree_resrc (resrc_tree_t *resrc_tree)
{
    if (resrc_tree)
        return (resrc_t *)resrc_tree->resrc;
    return NULL;
}

resrc_tree_list_t *resrc_tree_children (resrc_tree_t *resrc_tree)
{
    if (resrc_tree)
        return (resrc_tree_list_t *)resrc_tree->children;
    return NULL;
}

resrc_tree_t *resrc_tree_new (resrc_tree_t *parent, resrc_t *resrc)
{
    resrc_tree_t *resrc_tree = xzmalloc (sizeof (resrc_tree_t));
    if (resrc_tree) {
        resrc_tree->parent = parent;
        resrc_tree->resrc = resrc;
        resrc_tree->children = zlist_new ();
    } else {
        oom ();
    }

    return resrc_tree;
}

int resrc_tree_add_child (resrc_tree_t *tree, resrc_tree_t *child)
{
    int ret = -1;
    if (tree)
        ret = zlist_append (tree->children, child);

    return ret;
}

resrc_tree_t* resrc_tree_copy (resrc_tree_t *resrc_tree)
{
    resrc_tree_t *new_resrc_tree = xzmalloc (sizeof (resrc_tree_t));

    if (new_resrc_tree) {
        new_resrc_tree->parent = resrc_tree->parent;
        new_resrc_tree->children = zlist_dup (resrc_tree->children);
    } else {
        oom ();
    }

    return new_resrc_tree;
}

void resrc_tree_destroy (resrc_tree_t *resrc_tree)
{
    if (resrc_tree) {
        zlist_destroy (&resrc_tree->children);
        free (resrc_tree);
    }
}

void resrc_tree_print (resrc_tree_t *resrc_tree)
{
    resrc_tree_t *child;

    if (resrc_tree) {
        resrc_print_resource (resrc_tree->resrc);
        if (zlist_size (resrc_tree->children)) {
            child = zlist_first (resrc_tree->children);
            while (child) {
                resrc_tree_print (child);
                child = zlist_next (resrc_tree->children);
            }
        }
    }
}

/*
 * cycles through all of the resource children and returns true if all
 * of the sample children were found
 */
static int all_children_found (zlist_t * r_trees, zlist_t *s_trees,
                               bool available)
{
    resrc_tree_t *resrc_tree = zlist_first (r_trees);
    resrc_tree_t *sample_tree = zlist_first (s_trees);
    bool found = false;

    while (sample_tree && resrc_tree) {
        found = false;
        while (resrc_tree && !found) {
            if (resrc_match_resource (resrc_tree->resrc, sample_tree->resrc,
                                      available)) {
                if (zlist_size (sample_tree->children)) {
                    if (zlist_size (resrc_tree->children)) {
                        found = all_children_found (resrc_tree->children,
                                                    sample_tree->children,
                                                    available);
                    }
                } else {
                    found = true;
                }
            }
            /*
             * The following clause allows the sample tree to be sparsely
             * defined.  E.g., it might only stipulate a node with 4 cores
             * and omit the intervening socket.
             */
            if (!found)
                if (zlist_size (resrc_tree->children))
                    found = all_children_found (resrc_tree->children, s_trees,
                                                available);
            resrc_tree = zlist_next (r_trees);
        }
        if (!found)
            goto ret;
        sample_tree = zlist_next (s_trees);
    }
ret:
    return found;
}

/* returns the number of composites found */
int resrc_tree_search (resrc_tree_list_t *resrcs_in, resource_list_t *found_in,
                       resrc_tree_t *sample_tree, bool available)
{
    zlist_t * resrc_trees = (zlist_t*)resrcs_in;
    zlist_t * found = (zlist_t*)found_in;
    int nfound = 0;
    resrc_tree_t *resrc_tree;

    if (!resrc_trees || !found || !sample_tree) {
        goto ret;
    }

    resrc_tree = zlist_first (resrc_trees);
    while (resrc_tree) {
        if (resrc_match_resource (resrc_tree->resrc, sample_tree->resrc,
                                  available)) {
            if (zlist_size (sample_tree->children)) {
                if (zlist_size (resrc_tree->children) &&
                    all_children_found (resrc_tree->children,
                                        sample_tree->children, available)) {
                    zlist_append (found, strdup (resrc_name (
                                                     resrc_tree->resrc)));
                    nfound++;
                }
            } else {
                zlist_append (found, strdup (resrc_name (resrc_tree->resrc)));
                nfound++;
            }
        } else if (zlist_size (resrc_tree->children)) {
            nfound = resrc_tree_search (resrc_tree_children (resrc_tree),
                                        found_in, sample_tree, available);
        }
        resrc_tree = zlist_next (resrc_trees);
    }
ret:
    return nfound;
}


/*
 * vi: ts=4 sw=4 expandtab
 */

