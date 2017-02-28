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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <czmq.h>

#include "src/common/libutil/shortjansson.h"
#include "rdl.h"
#include "resrc_reqst.h"
#include "src/common/libutil/xzmalloc.h"

struct resrc_reqst_list {
    zlist_t *list;
};

struct resrc_reqst {
    resrc_reqst_t *parent;
    resrc_t *resrc;
    int64_t starttime;
    int64_t endtime;
    bool    exclusive;
    int64_t reqrd_qty;
    int64_t reqrd_size;
    int64_t nfound;
    resrc_reqst_list_t *children;
    resrc_graph_req_t *g_reqs;
};

static bool match_children (resrc_api_ctx_t *ctx, resrc_tree_list_t *r_trees,
                            resrc_reqst_list_t *req_trees,
                            resrc_tree_t *found_parent, bool available);

/***********************************************************************
 * Resource request
 ***********************************************************************/

void resrc_graph_req_destroy (resrc_api_ctx_t *ctx, resrc_graph_req_t *g_reqs)
{
    if (g_reqs)
        free (g_reqs);
}

resrc_t *resrc_reqst_resrc (resrc_reqst_t *resrc_reqst)
{
    if (resrc_reqst)
        return resrc_reqst->resrc;
    return NULL;
}

resrc_graph_req_t *resrc_reqst_graph_reqs (resrc_reqst_t *resrc_reqst)
{
    if (resrc_reqst)
        return resrc_reqst->g_reqs;
    return NULL;
}

int64_t resrc_reqst_starttime (resrc_reqst_t *resrc_reqst)
{
    if (resrc_reqst)
        return resrc_reqst->starttime;
    return -1;
}

int resrc_reqst_set_starttime (resrc_reqst_t *resrc_reqst, int64_t time)
{
    if (resrc_reqst) {
        resrc_reqst->starttime = time;
        if (resrc_reqst_num_children (resrc_reqst)) {
            resrc_reqst_t *child = resrc_reqst_list_first
                (resrc_reqst->children);
            while (child) {
                resrc_reqst_set_starttime (child, time);
                child = resrc_reqst_list_next (resrc_reqst->children);
            }
        }
        return 0;
    }
    return -1;
}

int64_t resrc_reqst_endtime (resrc_reqst_t *resrc_reqst)
{
    if (resrc_reqst)
        return resrc_reqst->endtime;
    return -1;
}

int resrc_reqst_set_endtime (resrc_reqst_t *resrc_reqst, int64_t time)
{
    if (resrc_reqst) {
        resrc_reqst->endtime = time;
        if (resrc_reqst_num_children (resrc_reqst)) {
            resrc_reqst_t *child = resrc_reqst_list_first
                (resrc_reqst->children);
            while (child) {
                resrc_reqst_set_endtime (child, time);
                child = resrc_reqst_list_next (resrc_reqst->children);
            }
        }
        return 0;
    }
    return -1;
}

bool resrc_reqst_exclusive (resrc_reqst_t *resrc_reqst)
{
    if (resrc_reqst)
        return resrc_reqst->exclusive;
    return false;
}

int64_t resrc_reqst_reqrd_qty (resrc_reqst_t *resrc_reqst)
{
    if (resrc_reqst)
        return resrc_reqst->reqrd_qty;
    return -1;
}

int64_t resrc_reqst_reqrd_size (resrc_reqst_t *resrc_reqst)
{
    if (resrc_reqst)
        return resrc_reqst->reqrd_size;
    return -1;
}

int64_t resrc_reqst_nfound (resrc_reqst_t *resrc_reqst)
{
    if (resrc_reqst)
        return resrc_reqst->nfound;
    return -1;
}

int64_t resrc_reqst_add_found (resrc_reqst_t *resrc_reqst, int64_t nfound)
{
    if (resrc_reqst) {
        resrc_reqst->nfound += nfound;
        return (resrc_reqst->nfound);
    }
    return -1;
}

int resrc_reqst_clear_found (resrc_reqst_t *resrc_reqst)
{
    if (resrc_reqst) {
        resrc_reqst->nfound = 0;
        if (resrc_reqst_num_children (resrc_reqst)) {
            resrc_reqst_t *child = resrc_reqst_list_first
                (resrc_reqst->children);
            while (child) {
                resrc_reqst_clear_found (child);
                child = resrc_reqst_list_next (resrc_reqst->children);
            }
        }
        return 0;
    }
    return -1;
}

bool resrc_reqst_all_found (resrc_reqst_t *resrc_reqst)
{
    bool all_found = false;

    if (resrc_reqst) {
        if (resrc_reqst_nfound (resrc_reqst) >=
            resrc_reqst_reqrd_qty (resrc_reqst))
            all_found = true;

        if (resrc_reqst_num_children (resrc_reqst)) {
            resrc_reqst_t *child = resrc_reqst_list_first(resrc_reqst->children);
            while (child) {
                if (!resrc_reqst_all_found (child)) {
                    all_found = false;
                    break;
                }
                child = resrc_reqst_list_next (resrc_reqst->children);
            }
        }
    }
    return all_found;
}

size_t resrc_reqst_num_children (resrc_reqst_t *resrc_reqst)
{
    if (resrc_reqst)
        return resrc_reqst_list_size (resrc_reqst->children);
    return 0;
}

resrc_reqst_list_t *resrc_reqst_children (resrc_reqst_t *resrc_reqst)
{
    if (resrc_reqst)
        return resrc_reqst->children;
    return NULL;
}

int resrc_reqst_add_child (resrc_reqst_t *parent, resrc_reqst_t *child)
{
    int rc = -1;
    if (parent) {
        child->parent = parent;
        rc = resrc_reqst_list_append (parent->children, child);
    }

    return rc;
}

resrc_reqst_t *resrc_reqst_new (resrc_t *resrc, int64_t qty, int64_t size,
                                int64_t starttime, int64_t endtime,
                                bool exclusive)
{
    resrc_reqst_t *resrc_reqst = xzmalloc (sizeof (resrc_reqst_t));
    if (resrc_reqst) {
        resrc_reqst->parent = NULL;
        resrc_reqst->resrc = resrc;
        resrc_reqst->starttime = starttime;
        resrc_reqst->endtime = endtime;
        resrc_reqst->exclusive = exclusive;
        resrc_reqst->reqrd_qty = qty;
        resrc_reqst->reqrd_size = size;
        resrc_reqst->nfound = 0;
        resrc_reqst->g_reqs = NULL;
        resrc_reqst->children = resrc_reqst_list_new ();
    }

    return resrc_reqst;
}

static resrc_graph_req_t *resrc_graph_req_from_json (json_t *ga)
{
    json_t *go = NULL;     /* graph json object */
    const char *name = NULL;
    int i, ngraphs = 0;
    int64_t ssize;
    resrc_graph_req_t *resrc_graph_req = NULL;

    if (Jget_ar_len (ga, &ngraphs)) {
        resrc_graph_req = xzmalloc ((ngraphs + 1) * sizeof (resrc_graph_req_t));
        for (i=0; i < ngraphs; i++) {
            Jget_ar_obj (ga, i, &go);
            if (Jget_str (go, "name", &name))
                resrc_graph_req[i].name = xstrdup (name);
            else
                goto fail;
            if (Jget_int64 (go, "size", &ssize))
                resrc_graph_req[i].size = (size_t) ssize;
            else
                goto fail;
        }
        /* end of the line */
        resrc_graph_req[i].name = NULL;
    }

    return resrc_graph_req;
fail:
    free (resrc_graph_req);
    return NULL;
}

resrc_reqst_t *resrc_reqst_from_json (resrc_api_ctx_t *ctx,
                                      json_t *o, resrc_reqst_t *parent)
{
    bool exclusive = false;
    json_t *ca = NULL;     /* array of child json objects */
    json_t *co = NULL;     /* child json object */
    json_t *ga = NULL;     /* array of graph json objects */
    int64_t endtime;
    int64_t qty = 0;
    int64_t size = 0;
    int64_t starttime;
    resrc_reqst_t *child_reqst = NULL;
    resrc_reqst_t *resrc_reqst = NULL;
    resrc_t *resrc = NULL;

    if (!Jget_int64 (o, "req_qty", &qty) && (qty < 1))
        goto ret;

    /*
     * If the size has not been specified, leave it at zero.  A size
     * of zero means that this job request will not consume any part
     * of the resource.  This allows multiple jobs to share the same
     * resource.
     */
    if (Jget_int64 (o, "req_size", &size) && (size < 0))
        goto ret;

    /*
     * If exclusivity has not been specified, leave it at false.
     */
    Jget_bool (o, "exclusive", &exclusive);

    /*
     * We use the request's start time to determine whether to request
     * resources that are available now or in the future.  A zero
     * starttime conveys a request for resources that are available
     * now.
     */
    if (parent)
        starttime = parent->starttime;
    else if (!(Jget_int64 (o, "starttime", &starttime)))
        starttime = 0;

    if (parent)
        endtime = parent->endtime;
    else if (!(Jget_int64 (o, "endtime", &endtime)))
        endtime = TIME_MAX;

    resrc = resrc_new_from_json (ctx, o, NULL, false);
    if (resrc) {
        resrc_reqst = resrc_reqst_new (resrc, qty, size, starttime, endtime,
                                       exclusive);

        if ((ga = Jobj_get (o, "graphs")))
            resrc_reqst->g_reqs = resrc_graph_req_from_json (ga);

        if ((co = Jobj_get (o, "req_child"))) {
            child_reqst = resrc_reqst_from_json (ctx, co, resrc_reqst);
            if (child_reqst)
                resrc_reqst_add_child (resrc_reqst, child_reqst);
        } else if ((ca = Jobj_get (o, "req_children"))) {
            int i, nchildren = 0;

            if (Jget_ar_len (ca, &nchildren)) {
                for (i=0; i < nchildren; i++) {
                    Jget_ar_obj (ca, i, &co);
                    child_reqst = resrc_reqst_from_json (ctx, co, resrc_reqst);
                    if (child_reqst)
                        resrc_reqst_add_child (resrc_reqst, child_reqst);
                }
            }
        }
    }
ret:
    return resrc_reqst;
}

void resrc_reqst_destroy (resrc_api_ctx_t *ctx, resrc_reqst_t *resrc_reqst)
{
    if (resrc_reqst) {
        if (resrc_reqst->parent)
            resrc_reqst_list_remove (resrc_reqst->parent->children, resrc_reqst);
        resrc_reqst_list_destroy (ctx, resrc_reqst->children);
        resrc_resource_destroy (ctx, resrc_reqst->resrc);
        resrc_graph_req_destroy (ctx, resrc_reqst->g_reqs);
        free (resrc_reqst);
    }
}

void resrc_graph_req_print (resrc_graph_req_t *graph_reqst)
{
    if (graph_reqst) {
        printf ("       requestng graphs of:");
        while (graph_reqst->name) {
            printf (" name: %s: size: %ld,", graph_reqst->name,
                    graph_reqst->size);
            graph_reqst++;
        }
        printf ("\n");
    }
}

void resrc_reqst_print (resrc_reqst_t *resrc_reqst)
{
    if (resrc_reqst) {
        char *shared = resrc_reqst->exclusive ? "exclusive" : "shared";

        printf ("%"PRId64" of %"PRId64" %s ", resrc_reqst->nfound,
                resrc_reqst->reqrd_qty, shared);
        resrc_print_resource (resrc_reqst->resrc);
        resrc_graph_req_print (resrc_reqst->g_reqs);
        if (resrc_reqst_num_children (resrc_reqst)) {
            resrc_reqst_t *child = resrc_reqst_list_first
                (resrc_reqst->children);
            while (child) {
                resrc_reqst_print (child);
                child = resrc_reqst_list_next (resrc_reqst->children);
            }
        }
    }
}

/***********************************************************************
 * Resource request list
 ***********************************************************************/

resrc_reqst_list_t *resrc_reqst_list_new ()
{
    resrc_reqst_list_t *new_list = xzmalloc (sizeof (resrc_reqst_list_t));
    new_list->list = zlist_new ();
    return new_list;
}

int resrc_reqst_list_append (resrc_reqst_list_t *rrl, resrc_reqst_t *rr)
{
    if (rrl && rrl->list && rr)
        return zlist_append  (rrl->list, (void *) rr);
    return -1;
}

resrc_reqst_t *resrc_reqst_list_first (resrc_reqst_list_t *rrl)
{
    if (rrl && rrl->list)
        return zlist_first (rrl->list);
    return NULL;
}

resrc_reqst_t *resrc_reqst_list_next (resrc_reqst_list_t *rrl)
{
    if (rrl && rrl->list)
        return zlist_next (rrl->list);
    return NULL;
}

size_t resrc_reqst_list_size (resrc_reqst_list_t *rrl)
{
    if (rrl && rrl->list)
        return zlist_size (rrl->list);
    return 0;
}

void resrc_reqst_list_remove (resrc_reqst_list_t *rrl, resrc_reqst_t *rr)
{
    zlist_remove (rrl->list, rr);
}

void resrc_reqst_list_destroy (resrc_api_ctx_t *ctx,
                               resrc_reqst_list_t *resrc_reqst_list)
{
    if (resrc_reqst_list) {
        if (resrc_reqst_list_size (resrc_reqst_list)) {
            resrc_reqst_t *child = resrc_reqst_list_first (resrc_reqst_list);
            while (child) {
                resrc_reqst_destroy (ctx, child);
                child = resrc_reqst_list_next (resrc_reqst_list);
            }
        }
        zlist_destroy (&(resrc_reqst_list->list));
        free (resrc_reqst_list);
    }
}

/*
 * cycles through all of the resource children and returns the number of
 * requested resources found
 */
static int64_t match_child (resrc_api_ctx_t *ctx,
                            resrc_tree_list_t *r_trees,
                            resrc_reqst_t *resrc_reqst,
                            resrc_tree_t *found_parent, bool available)
{
    int64_t nfound = 0;
    resrc_t *resrc = NULL;
    resrc_tree_t *resrc_tree = NULL;

    resrc_tree = resrc_tree_list_first (r_trees);
    while (resrc_tree) {
        resrc = resrc_tree_resrc (resrc_tree);
        nfound += resrc_tree_search (ctx, resrc, resrc_reqst, &found_parent,
                                     available);
        resrc_tree = resrc_tree_list_next (r_trees);
    }

    return nfound;
}

/*
 * cycles through all of the resource requests and returns true if all
 * of the requested children were found
 */
static bool match_children (resrc_api_ctx_t *ctx,
                            resrc_tree_list_t *r_trees,
                            resrc_reqst_list_t *req_trees,
                            resrc_tree_t *found_parent, bool available)
{
    bool found = false;
    resrc_reqst_t *resrc_reqst = resrc_reqst_list_first (req_trees);

    while (resrc_reqst) {
        resrc_reqst->nfound = 0;
        found = false;

        if (match_child (ctx, r_trees, resrc_reqst, found_parent, available)) {
            if (resrc_reqst->nfound >= resrc_reqst->reqrd_qty)
                found = true;
        }
        if (!found)
            break;

        resrc_reqst = resrc_reqst_list_next (req_trees);
    }

    return found;
}

/* returns the number of resource or resource composites found */
int64_t resrc_tree_search (resrc_api_ctx_t *ctx,
                           resrc_t *resrc_in, resrc_reqst_t *resrc_reqst,
                           resrc_tree_t **found_tree, bool available)
{
    int64_t nfound = 0;
    resrc_tree_list_t *children = NULL;
    resrc_tree_t *child_tree;
    resrc_tree_t *new_tree = NULL;

    if (!resrc_in || !found_tree || !resrc_reqst) {
        goto ret;
    }

    if (resrc_match_resource (resrc_in, resrc_reqst, available)) {
        if (resrc_reqst_num_children (resrc_reqst)) {
            if (resrc_tree_num_children (resrc_phys_tree (resrc_in))) {
                new_tree = resrc_tree_new (*found_tree, resrc_in);
                children = resrc_tree_children (resrc_phys_tree (resrc_in));
                if (match_children (ctx, children, resrc_reqst->children, new_tree,
                                    available)) {
                    nfound = 1;
                    resrc_reqst->nfound++;
                } else {
                    resrc_tree_destroy (ctx, new_tree, false, false);
                }
            }
        } else {
            (void) resrc_tree_new (*found_tree, resrc_in);
            nfound = 1;
            resrc_reqst->nfound++;
        }
    } else if (resrc_tree_num_children (resrc_phys_tree (resrc_in))) {
        /*
         * This clause visits the children of the current resource
         * searching for a match to the resource request.  The found
         * tree must be extended to include this intermediate
         * resource.
         *
         * This also allows the resource request to be sparsely
         * defined.  E.g., it might only stipulate a node with 4 cores
         * and omit the intervening socket.
         */
        if (*found_tree)
            new_tree = resrc_tree_new (*found_tree, resrc_in);
        else {
            new_tree = resrc_tree_new (NULL, resrc_in);
            *found_tree = new_tree;
        }
        children = resrc_tree_children (resrc_phys_tree (resrc_in));
        child_tree = resrc_tree_list_first (children);
        while (child_tree) {
            nfound += resrc_tree_search (ctx, resrc_tree_resrc (child_tree),
                                         resrc_reqst, &new_tree, available);
            child_tree = resrc_tree_list_next (children);
        }
    }
ret:
    return nfound;
}

/*
 * vi: ts=4 sw=4 expandtab
 */

