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
    resrc_tree_t parent;
    resrc_t resrc;
    zlist_t *children;
};

/***********************************************************************
 * Resource tree
 ***********************************************************************/

resrc_t resrc_tree_resrc (resrc_tree_t resrc_tree)
{
    if (resrc_tree)
        return (resrc_t)resrc_tree->resrc;
    return NULL;
}

size_t resrc_tree_num_children (resrc_tree_t resrc_tree)
{
    if (resrc_tree)
        return zlist_size (resrc_tree->children);
    return -1;
}

resrc_tree_list_t resrc_tree_children (resrc_tree_t resrc_tree)
{
    if (resrc_tree)
        return (resrc_tree_list_t)resrc_tree->children;
    return NULL;
}

int resrc_tree_add_child (resrc_tree_t parent, resrc_tree_t child)
{
    int rc = -1;
    if (parent)
        rc = zlist_append (parent->children, child);

    return rc;
}

resrc_tree_t resrc_tree_new (resrc_tree_t parent, resrc_t resrc)
{
    resrc_tree_t resrc_tree = xzmalloc (sizeof (struct resrc_tree));
    if (resrc_tree) {
        resrc_tree->parent = parent;
        resrc_tree->resrc = resrc;
        resrc_tree->children = zlist_new ();
        (void) resrc_tree_add_child (parent, resrc_tree);
    } else {
        oom ();
    }

    return resrc_tree;
}

resrc_tree_t resrc_tree_copy (resrc_tree_t resrc_tree)
{
    resrc_tree_t new_resrc_tree = xzmalloc (sizeof (struct resrc_tree));

    if (new_resrc_tree) {
        new_resrc_tree->parent = resrc_tree->parent;
        new_resrc_tree->children = zlist_dup (resrc_tree->children);
    } else {
        oom ();
    }

    return new_resrc_tree;
}

void resrc_tree_free (resrc_tree_t resrc_tree)
{
    if (resrc_tree) {
        zlist_destroy (&resrc_tree->children);
        free (resrc_tree);
    }
}

void resrc_tree_destroy (resrc_tree_t resrc_tree)
{
    resrc_tree_t child;

    if (resrc_tree) {
        if (resrc_tree->parent)
            zlist_remove (resrc_tree->parent->children, resrc_tree);
        if (zlist_size (resrc_tree->children)) {
            child = zlist_first (resrc_tree->children);
            while (child) {
                resrc_tree_destroy (child);
                child = zlist_next (resrc_tree->children);
            }
        }
        zlist_destroy (&resrc_tree->children);
        free (resrc_tree);
    }
}

void resrc_tree_print (resrc_tree_t resrc_tree)
{
    resrc_tree_t child;

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

int resrc_tree_serialize (JSON o, resrc_tree_t rt)
{
    int rc = -1;

    if (o && rt) {
        rc = 0;
        if (zlist_size (rt->children)) {
            JSON co = Jnew ();
            json_object_object_add (o, resrc_name (rt->resrc), co);
            resrc_tree_t child = zlist_first (rt->children);
            while (!rc && child) {
                rc = resrc_tree_serialize (co, child);
                child = zlist_next (rt->children);
            }
        } else {
            rc = resrc_to_json (o, rt->resrc);
        }
    }
    return rc;
}

int resrc_tree_allocate (resrc_tree_t rt, int64_t job_id)
{
    int rc = -1;
    if (rt) {
        rc = resrc_allocate_resource (rt->resrc, job_id);
        if (zlist_size (rt->children)) {
            resrc_tree_t child = zlist_first (rt->children);
            while (!rc && child) {
                rc = resrc_tree_allocate (child, job_id);
                child = zlist_next (rt->children);
            }
        }
    }
    return rc;
}

int resrc_tree_reserve (resrc_tree_t rt, int64_t job_id)
{
    int rc = -1;
    if (rt) {
        rc = resrc_reserve_resource (rt->resrc, job_id);
        if (zlist_size (rt->children)) {
            resrc_tree_t child = zlist_first (rt->children);
            while (!rc && child) {
                rc = resrc_tree_reserve (child, job_id);
                child = zlist_next (rt->children);
            }
        }
    }
    return rc;
}

int resrc_tree_release (resrc_tree_t rt, int64_t job_id)
{
    int rc = -1;
    if (rt) {
        rc = resrc_release_resource (rt->resrc, job_id);
        if (zlist_size (rt->children)) {
            resrc_tree_t child = zlist_first (rt->children);
            while (!rc && child) {
                rc = resrc_tree_release (child, job_id);
                child = zlist_next (rt->children);
            }
        }
    }
    return rc;
}

/***********************************************************************
 * Resource tree list
 ***********************************************************************/

resrc_tree_list_t resrc_tree_new_list ()
{
    return (resrc_tree_list_t) zlist_new ();
}

int resrc_tree_list_append (resrc_tree_list_t rtl, resrc_tree_t rt)
{
    if (rtl && rt)
        return zlist_append ((zlist_t *) rtl, (void *) rt);
    return -1;
}

resrc_tree_t resrc_tree_list_first (resrc_tree_list_t rtl)
{
    return zlist_first ((zlist_t*) rtl);
}

resrc_tree_t resrc_tree_list_next (resrc_tree_list_t rtl)
{
    return zlist_next ((zlist_t*) rtl);
}

size_t resrc_tree_list_size (resrc_tree_list_t rtl)
{
    return zlist_size ((zlist_t*) rtl);
}

void resrc_tree_list_destroy (resrc_tree_list_t resrc_tree_list)
{
    zlist_t *child;
    zlist_t *rtl = (zlist_t*) resrc_tree_list;

    if (rtl) {
        if (zlist_size (rtl)) {
            child = zlist_first (rtl);
            while (child) {
                resrc_tree_destroy ((resrc_tree_t) child);
                child = zlist_next (rtl);
            }
        }
        zlist_destroy (&rtl);
        free (rtl);
    }
}

int resrc_tree_list_serialize (JSON o, resrc_tree_list_t rtl)
{
    resrc_tree_t rt;
    int rc = -1;

    if (o && rtl) {
        rc = 0;
        rt = resrc_tree_list_first (rtl);
        while (!rc && rt) {
            rc = resrc_tree_serialize (o, rt);
            rt = resrc_tree_list_next (rtl);
        }
    }

    return rc;
}

int resrc_tree_list_allocate (resrc_tree_list_t rtl, int64_t job_id)
{
    resrc_tree_t rt;
    int rc = -1;

    if (rtl) {
        rc = 0;
        rt = resrc_tree_list_first (rtl);
        while (!rc && rt) {
            rc = resrc_tree_allocate (rt, job_id);
            rt = resrc_tree_list_next (rtl);
        }
    }

    return rc;
}

int resrc_tree_list_reserve (resrc_tree_list_t rtl, int64_t job_id)
{
    resrc_tree_t rt;
    int rc = -1;

    if (rtl) {
        rc = 0;
        rt = resrc_tree_list_first (rtl);
        while (!rc && rt) {
            rc = resrc_tree_reserve (rt, job_id);
            rt = resrc_tree_list_next (rtl);
        }
    }

    return rc;
}

int resrc_tree_list_release (resrc_tree_list_t rtl, int64_t job_id)
{
    resrc_tree_t rt;
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

/*
 * vi: ts=4 sw=4 expandtab
 */

