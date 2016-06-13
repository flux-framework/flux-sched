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

#include "src/common/libutil/shortjson.h"
#include "rdl.h"
#include "resrc_tree.h"
#include "src/common/libutil/xzmalloc.h"

struct resrc_tree_list {
    zlist_t *list;
};


struct resrc_tree {
    resrc_tree_t *parent;
    resrc_t *resrc;
    resrc_tree_list_t *children;
};

/***********************************************************************
 * Resource tree
 ***********************************************************************/

resrc_t *resrc_tree_resrc (resrc_tree_t *resrc_tree)
{
    if (resrc_tree)
        return resrc_tree->resrc;
    return NULL;
}

size_t resrc_tree_num_children (resrc_tree_t *resrc_tree)
{
    if (resrc_tree)
        return resrc_tree_list_size (resrc_tree->children);
    return 0;
}

resrc_tree_list_t *resrc_tree_children (resrc_tree_t *resrc_tree)
{
    if (resrc_tree)
        return resrc_tree->children;
    return NULL;
}

int resrc_tree_add_child (resrc_tree_t *parent, resrc_tree_t *child)
{
    int rc = -1;
    if (parent) {
        child->parent = parent;
        rc = resrc_tree_list_append (parent->children, child);
    }

    return rc;
}

resrc_tree_t *resrc_tree_new (resrc_tree_t *parent, resrc_t *resrc)
{
    resrc_tree_t *resrc_tree = xzmalloc (sizeof (resrc_tree_t));
    if (resrc_tree) {
        resrc_tree->parent = parent;
        resrc_tree->resrc = resrc;
        resrc_tree->children = resrc_tree_list_new ();
        if (parent)
            (void) resrc_tree_add_child (parent, resrc_tree);
    }

    return resrc_tree;
}

resrc_tree_t *resrc_tree_copy (resrc_tree_t *resrc_tree)
{
    resrc_tree_t *new_resrc_tree = xzmalloc (sizeof (resrc_tree_t));

    if (new_resrc_tree) {
        new_resrc_tree->parent = resrc_tree->parent;
        new_resrc_tree->resrc = resrc_tree->resrc;
        new_resrc_tree->children = resrc_tree_list_new ();
        new_resrc_tree->children->list = zlist_dup (resrc_tree->children->list);
    }

    return new_resrc_tree;
}

void resrc_tree_destroy (resrc_tree_t *resrc_tree, bool destroy_resrc)
{
    if (resrc_tree) {
        resrc_tree_t *phys_tree = resrc_phys_tree (resrc_tree->resrc);

        if (resrc_tree->parent)
            resrc_tree_list_remove (resrc_tree->parent->children, resrc_tree);
        if (resrc_tree->children) {
            resrc_tree_list_destroy (resrc_tree->children, destroy_resrc);
            resrc_tree->children = NULL;
        }

        if (destroy_resrc)
            resrc_resource_destroy (resrc_tree->resrc);
        free (resrc_tree);
    }
}

void resrc_tree_print (resrc_tree_t *resrc_tree)
{
    if (resrc_tree) {
        resrc_print_resource (resrc_tree->resrc);
        if (resrc_tree_num_children (resrc_tree)) {
            resrc_tree_t *child = resrc_tree_list_first (resrc_tree->children);
            while (child) {
                resrc_tree_print (child);
                child = resrc_tree_list_next (resrc_tree->children);
            }
        }
    }
}

int resrc_tree_serialize (JSON o, resrc_tree_t *resrc_tree)
{
    int rc = -1;

    if (o && resrc_tree) {
        rc = resrc_to_json (o, resrc_tree->resrc);
        if (!rc && resrc_tree_num_children (resrc_tree)) {
            JSON ja = Jnew_ar ();

            if (!(rc = resrc_tree_list_serialize (ja, resrc_tree->children)))
                json_object_object_add (o, "children", ja);
        }
    }
    return rc;
}

resrc_tree_t *resrc_tree_deserialize (JSON o, resrc_tree_t *parent)
{
    JSON ca = NULL;     /* array of child json objects */
    JSON co = NULL;     /* child json object */
    resrc_t *resrc = NULL;
    resrc_tree_t *resrc_tree = NULL;

    resrc = resrc_new_from_json (o, NULL, false);
    if (resrc) {
        resrc_tree = resrc_tree_new (parent, resrc);

        if ((ca = Jobj_get (o, "children"))) {
            int i, nchildren = 0;

            if (Jget_ar_len (ca, &nchildren)) {
                for (i=0; i < nchildren; i++) {
                    Jget_ar_obj (ca, i, &co);
                    (void) resrc_tree_deserialize (co, resrc_tree);
                }
            }
        }
    }

    return resrc_tree;
}

int resrc_tree_allocate (resrc_tree_t *resrc_tree, int64_t job_id,
                         int64_t starttime, int64_t endtime)
{
    int rc = -1;
    if (resrc_tree) {
        rc = resrc_allocate_resource (resrc_tree->resrc, job_id,
                                      starttime, endtime);
        if (resrc_tree_num_children (resrc_tree))
            rc = resrc_tree_list_allocate (resrc_tree->children, job_id,
                                           starttime, endtime);
    }
    return rc;
}

int resrc_tree_reserve (resrc_tree_t *resrc_tree, int64_t job_id,
                        int64_t starttime, int64_t endtime)
{
    int rc = -1;
    if (resrc_tree) {
        rc = resrc_reserve_resource (resrc_tree->resrc, job_id,
                                     starttime, endtime);
        if (resrc_tree_num_children (resrc_tree))
            rc = resrc_tree_list_reserve (resrc_tree->children, job_id,
                                          starttime, endtime);
    }
    return rc;
}

int resrc_tree_release (resrc_tree_t *resrc_tree, int64_t job_id)
{
    int rc = -1;
    if (resrc_tree) {
        rc = resrc_release_allocation (resrc_tree->resrc, job_id);
        if (resrc_tree_num_children (resrc_tree))
            rc = resrc_tree_list_release (resrc_tree->children, job_id);
    }
    return rc;
}

int resrc_tree_release_all_reservations (resrc_tree_t *resrc_tree)
{
    int rc = -1;
    if (resrc_tree) {
        rc = resrc_release_all_reservations (resrc_tree->resrc);
        if (resrc_tree_num_children (resrc_tree))
            rc = resrc_tree_list_release_all_reservations (resrc_tree->children);
    }
    return rc;
}

void resrc_tree_unstage_resources (resrc_tree_t *resrc_tree)
{
    if (resrc_tree) {
        resrc_stage_resrc (resrc_tree->resrc, 0);
        if (resrc_tree_num_children (resrc_tree))
            resrc_tree_list_unstage_resources (resrc_tree->children);
    }
}

/***********************************************************************
 * Resource tree list
 ***********************************************************************/

resrc_tree_list_t *resrc_tree_list_new ()
{
    resrc_tree_list_t *new_list = xzmalloc (sizeof (resrc_tree_list_t));
    new_list->list = zlist_new ();
    return new_list;
}

int resrc_tree_list_append (resrc_tree_list_t *rtl, resrc_tree_t *rt)
{
    if (rtl && rtl->list && rt)
        return zlist_append (rtl->list, (void *) rt);
    return -1;
}

resrc_tree_t *resrc_tree_list_first (resrc_tree_list_t *rtl)
{
    if (rtl && rtl->list)
        return zlist_first (rtl->list);
    return NULL;
}

resrc_tree_t *resrc_tree_list_next (resrc_tree_list_t *rtl)
{
    if (rtl && rtl->list)
        return zlist_next (rtl->list);
    return NULL;
}

size_t resrc_tree_list_size (resrc_tree_list_t *rtl)
{
    if (rtl && rtl->list)
        return zlist_size (rtl->list);
    return 0;
}

void resrc_tree_list_remove (resrc_tree_list_t *rtl, resrc_tree_t *rt)
{
    zlist_remove (rtl->list, rt);
}

void resrc_tree_list_free (resrc_tree_list_t *resrc_tree_list)
{
    if (resrc_tree_list) {
        if (resrc_tree_list->list) {
            zlist_destroy (&(resrc_tree_list->list));
        }
        free (resrc_tree_list);
    }
}

void resrc_tree_list_destroy (resrc_tree_list_t *resrc_tree_list,
                              bool destroy_resrc)
{
    if (resrc_tree_list) {
        if (resrc_tree_list_size (resrc_tree_list)) {
            resrc_tree_t *child = resrc_tree_list_first (resrc_tree_list);
            while (child) {
                resrc_tree_destroy (child, destroy_resrc);
                child = resrc_tree_list_next (resrc_tree_list);
            }
        }
        resrc_tree_list_free (resrc_tree_list);
    }
}

int resrc_tree_list_serialize (JSON o, resrc_tree_list_t *rtl)
{
    resrc_tree_t *rt;
    int rc = -1;

    if (o && rtl && rtl->list) {
        rc = 0;
        rt = resrc_tree_list_first (rtl);
        while (rt) {
            JSON co = Jnew ();

            if ((rc = resrc_tree_serialize (co, rt)))
                break;
            json_object_array_add (o, co);
            rt = resrc_tree_list_next (rtl);
        }
    }

    return rc;
}

resrc_tree_list_t *resrc_tree_list_deserialize (JSON o)
{
    JSON ca = NULL;     /* array of child json objects */
    int i, nchildren = 0;
    resrc_tree_t *rt = NULL;
    resrc_tree_list_t *rtl = resrc_tree_list_new ();

    if (o && rtl) {
        if (Jget_ar_len (o, &nchildren)) {
            for (i=0; i < nchildren; i++) {
                Jget_ar_obj (o, i, &ca);
                rt = resrc_tree_deserialize (ca, NULL);
                if (!rt || resrc_tree_list_append (rtl, rt))
                    break;
            }
        }
    }

    return rtl;
}

int resrc_tree_list_allocate (resrc_tree_list_t *rtl, int64_t job_id,
                              int64_t starttime, int64_t endtime)
{
    resrc_tree_t *rt;
    int rc = -1;

    if (rtl) {
        rc = 0;
        rt = resrc_tree_list_first (rtl);
        while (!rc && rt) {
            rc = resrc_tree_allocate (rt, job_id, starttime, endtime);
            rt = resrc_tree_list_next (rtl);
        }
    }

    return rc;
}

int resrc_tree_list_reserve (resrc_tree_list_t *rtl, int64_t job_id,
                             int64_t starttime, int64_t endtime)
{
    resrc_tree_t *rt;
    int rc = -1;

    if (rtl) {
        rc = 0;
        rt = resrc_tree_list_first (rtl);
        while (!rc && rt) {
            rc = resrc_tree_reserve (rt, job_id, starttime, endtime);
            rt = resrc_tree_list_next (rtl);
        }
    }

    return rc;
}

int resrc_tree_list_release (resrc_tree_list_t *rtl, int64_t job_id)
{
    resrc_tree_t *rt;
    int rc = -1;

    if (rtl) {
        rc = 0;
        rt = resrc_tree_list_first (rtl);
        while (!rc && rt) {
            rc = resrc_tree_release (rt, job_id);
            rt = resrc_tree_list_next (rtl);
        }
    }

    return rc;
}

int resrc_tree_list_release_all_reservations (resrc_tree_list_t *rtl)
{
    resrc_tree_t *rt;
    int rc = -1;

    if (rtl) {
        rc = 0;
        rt = resrc_tree_list_first (rtl);
        while (!rc && rt) {
            rc = resrc_tree_release_all_reservations (rt);
            rt = resrc_tree_list_next (rtl);
        }
    }

    return rc;
}

void resrc_tree_list_unstage_resources (resrc_tree_list_t *rtl)
{
    resrc_tree_t *rt;

    if (rtl) {
        rt = resrc_tree_list_first (rtl);
        while (rt) {
            resrc_tree_unstage_resources (rt);
            rt = resrc_tree_list_next (rtl);
        }
    }
}

/*
 * vi: ts=4 sw=4 expandtab
 */

