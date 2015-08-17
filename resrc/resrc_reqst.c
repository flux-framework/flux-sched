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
#include "resrc_reqst.h"
#include "src/common/libutil/xzmalloc.h"

typedef struct zlist_t resrc_reqst_list;

struct resrc_reqst {
    resrc_reqst_t parent;
    resrc_t resrc;
    int64_t reqrd;
    int64_t nfound;
    zlist_t *children;
};

static bool match_children (resrc_tree_list_t r_trees, zlist_t *req_trees,
                            resrc_tree_t parent_tree, bool available);

/***********************************************************************
 * Resource request
 ***********************************************************************/

resrc_t resrc_reqst_resrc (resrc_reqst_t resrc_reqst)
{
    if (resrc_reqst)
        return (resrc_t)resrc_reqst->resrc;
    return NULL;
}

int64_t resrc_reqst_reqrd (resrc_reqst_t resrc_reqst)
{
    if (resrc_reqst)
        return resrc_reqst->reqrd;
    return -1;
}

int64_t resrc_reqst_nfound (resrc_reqst_t resrc_reqst)
{
    if (resrc_reqst)
        return resrc_reqst->nfound;
    return -1;
}

int64_t resrc_reqst_add_found (resrc_reqst_t resrc_reqst, int64_t nfound)
{
    if (resrc_reqst) {
        resrc_reqst->nfound += nfound;
        return (resrc_reqst->nfound);
    }
    return -1;
}

void resrc_reqst_clear_found (resrc_reqst_t resrc_reqst)
{
    resrc_reqst_t child;

    if (resrc_reqst) {
        resrc_reqst->nfound = 0;
        if (zlist_size (resrc_reqst->children)) {
            child = zlist_first (resrc_reqst->children);
            while (child) {
                resrc_reqst_clear_found (child);
                child = zlist_next (resrc_reqst->children);
            }
        }
    }
}

size_t resrc_reqst_num_children (resrc_reqst_t resrc_reqst)
{
    if (resrc_reqst)
        return zlist_size (resrc_reqst->children);
    return -1;
}

resrc_reqst_list_t resrc_reqst_children (resrc_reqst_t resrc_reqst)
{
    if (resrc_reqst)
        return (resrc_reqst_list_t)resrc_reqst->children;
    return NULL;
}

int resrc_reqst_add_child (resrc_reqst_t parent, resrc_reqst_t child)
{
    int rc = -1;
    if (parent) {
        child->parent = parent;
        rc = zlist_append (parent->children, child);
    }

    return rc;
}

resrc_reqst_t resrc_reqst_new (resrc_t resrc, int64_t qty)
{
    resrc_reqst_t resrc_reqst = xzmalloc (sizeof (struct resrc_reqst));
    if (resrc_reqst) {
        resrc_reqst->parent = NULL;
        resrc_reqst->resrc = resrc;
        resrc_reqst->reqrd = qty;
        resrc_reqst->nfound = 0;
        resrc_reqst->children = zlist_new ();
    } else {
        oom ();
    }

    return resrc_reqst;
}

resrc_reqst_t resrc_reqst_from_json (JSON o, resrc_t parent)
{
    int qty = 0;
    JSON ca = NULL;     /* array of child json objects */
    JSON co = NULL;     /* child json object */
    resrc_t resrc = NULL;
    resrc_reqst_t child_reqst = NULL;
    resrc_reqst_t resrc_reqst = NULL;

    if (!Jget_int (o, "req_qty", &qty) && (qty < 1))
        goto ret;

    resrc = resrc_new_from_json (o, parent);
    if (resrc) {
        resrc_reqst = resrc_reqst_new (resrc, qty);

        if ((co = Jobj_get (o, "req_child"))) {
            child_reqst = resrc_reqst_from_json (co, resrc);
            if (child_reqst)
                resrc_reqst_add_child (resrc_reqst, child_reqst);
        } else if ((ca = Jobj_get (o, "req_children"))) {
            int i, nchildren = 0;

            if (Jget_ar_len (ca, &nchildren)) {
                for (i=0; i < nchildren; i++) {
                    Jget_ar_obj (ca, i, &co);
                    child_reqst = resrc_reqst_from_json (co, resrc);
                    if (child_reqst)
                        resrc_reqst_add_child (resrc_reqst, child_reqst);
                }
            }
        }
    }
ret:
    return resrc_reqst;
}

void resrc_reqst_print (resrc_reqst_t resrc_reqst)
{
    resrc_reqst_t child;

    if (resrc_reqst) {
        printf ("%ld of %ld ", resrc_reqst->nfound, resrc_reqst->reqrd);
        resrc_print_resource (resrc_reqst->resrc);
        if (zlist_size (resrc_reqst->children)) {
            child = zlist_first (resrc_reqst->children);
            while (child) {
                resrc_reqst_print (child);
                child = zlist_next (resrc_reqst->children);
            }
        }
    }
}

void resrc_reqst_destroy (resrc_reqst_t resrc_reqst)
{
    resrc_reqst_t child;

    if (resrc_reqst) {
        if (resrc_reqst->parent)
            zlist_remove (resrc_reqst->parent->children, resrc_reqst);
        if (zlist_size (resrc_reqst->children)) {
            child = zlist_first (resrc_reqst->children);
            while (child) {
                resrc_reqst_destroy (child);
                child = zlist_next (resrc_reqst->children);
            }
        }
        zlist_destroy (&resrc_reqst->children);
        resrc_resource_destroy (resrc_reqst->resrc);
        free (resrc_reqst);
    }
}

/***********************************************************************
 * Resource request list
 ***********************************************************************/

resrc_reqst_list_t resrc_reqst_new_list ()
{
    return (resrc_reqst_list_t) zlist_new ();
}

int resrc_reqst_list_append (resrc_reqst_list_t rrl, resrc_reqst_t rr)
{
    if (rrl && rr)
        return zlist_append ((zlist_t *) rrl, (void *) rr);
    return -1;
}

resrc_reqst_t resrc_reqst_list_first (resrc_reqst_list_t rrl)
{
    return zlist_first ((zlist_t*) rrl);
}

resrc_reqst_t resrc_reqst_list_next (resrc_reqst_list_t rrl)
{
    return zlist_next ((zlist_t*) rrl);
}

size_t resrc_reqst_list_size (resrc_reqst_list_t rrl)
{
    return zlist_size ((zlist_t*) rrl);
}

void resrc_reqst_list_destroy (resrc_reqst_list_t resrc_reqst_list)
{
    zlist_t *child;
    zlist_t *rtl = (zlist_t*) resrc_reqst_list;

    if (rtl) {
        if (zlist_size (rtl)) {
            child = zlist_first (rtl);
            while (child) {
                resrc_reqst_destroy ((resrc_reqst_t) child);
                child = zlist_next (rtl);
            }
        }
        zlist_destroy (&rtl);
        free (rtl);
    }
}


static bool match_child (resrc_tree_list_t r_trees, resrc_reqst_t resrc_reqst,
                         resrc_tree_t parent_tree, bool available)
{
    resrc_tree_t resrc_tree = NULL;
    resrc_tree_t child_tree = NULL;
    bool found = false;
    bool success = false;

    resrc_tree = resrc_tree_list_first (r_trees);
    while (resrc_tree) {
        found = false;
        if (resrc_match_resource (resrc_tree_resrc (resrc_tree),
                                  resrc_reqst->resrc, available)) {
            if (zlist_size (resrc_reqst->children)) {
                if (resrc_tree_num_children (resrc_tree)) {
                    child_tree = resrc_tree_new (parent_tree,
                                                 resrc_tree_resrc (resrc_tree));
                    if (match_children (resrc_tree_children (resrc_tree),
                                        resrc_reqst->children, child_tree,
                                        available)) {
                        resrc_reqst->nfound++;
                        found = true;
                        success = true;
                    } else {
                        resrc_tree_destroy (child_tree);
                    }
                }
            } else {
                (void) resrc_tree_new (parent_tree,
                                       resrc_tree_resrc (resrc_tree));
                resrc_reqst->nfound++;
                found = true;
                success = true;
            }
        }
        /*
         * The following clause allows the resource request to be
         * sparsely defined.  E.g., it might only stipulate a node
         * with 4 cores and omit the intervening socket.
         */
        if (!found) {
            if (resrc_tree_num_children (resrc_tree)) {
                child_tree = resrc_tree_new (parent_tree,
                                             resrc_tree_resrc (resrc_tree));
                if (match_child (resrc_tree_children (resrc_tree), resrc_reqst,
                                 child_tree, available)) {
                    success = true;
                } else {
                    resrc_tree_destroy (child_tree);
                }
            }
        }
        resrc_tree = resrc_tree_list_next (r_trees);
    }

    return success;
}

/*
 * cycles through all of the resource children and returns true if all
 * of the requested children were found
 */
static bool match_children (resrc_tree_list_t r_trees, zlist_t *req_trees,
                            resrc_tree_t parent_tree, bool available)
{
    resrc_reqst_t resrc_reqst = zlist_first (req_trees);
    bool found = false;

    while (resrc_reqst) {
        resrc_reqst->nfound = 0;
        found = false;

        if (match_child (r_trees, resrc_reqst, parent_tree, available)) {
            if (resrc_reqst->nfound >= resrc_reqst->reqrd)
                found = true;
        }
        if (!found)
            break;

        resrc_reqst = zlist_next (req_trees);
    }

    return found;
}

/* returns the number of composites found */
int resrc_tree_search (resrc_tree_list_t resrcs_in, resrc_reqst_t resrc_reqst,
                       resrc_tree_list_t found_in, bool available)
{
    int64_t nfound = 0;
    resrc_tree_t new_tree = NULL;
    resrc_tree_t resrc_tree;
    zlist_t *found_trees = (zlist_t*)found_in;

    if (!resrcs_in || !found_trees || !resrc_reqst) {
        goto ret;
    }

    resrc_tree = resrc_tree_list_first (resrcs_in);
    while (resrc_tree) {
        if (resrc_match_resource (resrc_tree_resrc (resrc_tree),
                                  resrc_reqst->resrc, available)) {
            if (zlist_size (resrc_reqst->children)) {
                if (resrc_tree_num_children (resrc_tree)) {
                    new_tree = resrc_tree_new (NULL,
                                               resrc_tree_resrc (resrc_tree));
                    if (match_children (resrc_tree_children (resrc_tree),
                                        resrc_reqst->children, new_tree,
                                        available)) {
                        zlist_append (found_trees, new_tree);
                        nfound++;
                    } else {
                        resrc_tree_destroy (new_tree);
                    }
                }
            } else {
                new_tree = resrc_tree_new (NULL, resrc_tree_resrc (resrc_tree));
                zlist_append (found_trees, new_tree);
                nfound++;
            }
        } else if (resrc_tree_num_children (resrc_tree)) {
            nfound += resrc_tree_search (resrc_tree_children (resrc_tree),
                                         resrc_reqst, found_in, available);
        }
        resrc_tree = resrc_tree_list_next (resrcs_in);
    }
ret:
    return nfound;
}

/*
 * vi: ts=4 sw=4 expandtab
 */

