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
#include "resrc_flow.h"
#include "resrc_reqst.h"
#include "src/common/libutil/xzmalloc.h"

struct resrc_flow_list {
    zlist_t *list;
};

/*
 * A resrc_flow structure requires the following members to represent
 * flow that reaches a resource and is independent from the attributes
 * of the physical resource (resrc_t members).  For example, a compute
 * node resource could receive power from an enclosure which is
 * powered by a pdu.  Hence the node's "flow resource" models
 * electrical current and is a member of the power flow hierarchy
 * while the associated compute node resource is part of the physical
 * hierarchy with separate parentage.
 *
 *  char *path;
 *  char *basename;
 *  char *name;
 *  size_t size;
 *  size_t available;
 *  size_t staged;
 *  zhash_t *allocs;
 *  zhash_t *reservtns;
 *  planner_t *twindow;
 *
 * The resrc_flow structure therefore includes a flow_resrc resource,
 * independent from the associated resource, to hold all these values
 * and allow us to leverage the resrc's time window search code.
 */

struct resrc_flow {
    resrc_flow_t *parent;
    resrc_t *resrc;
    resrc_t *flow_resrc;
    resrc_flow_list_t *children;
};

/***********************************************************************
 * Resource flow
 ***********************************************************************/

resrc_t *resrc_flow_resrc (resrc_flow_t *resrc_flow)
{
    if (resrc_flow)
        return resrc_flow->resrc;
    return NULL;
}

resrc_t *resrc_flow_flow_resrc (resrc_flow_t *resrc_flow)
{
    if (resrc_flow)
        return resrc_flow->flow_resrc;
    return NULL;
}

size_t resrc_flow_num_children (resrc_flow_t *resrc_flow)
{
    if (resrc_flow)
        return resrc_flow_list_size (resrc_flow->children);
    return 0;
}

resrc_flow_list_t *resrc_flow_children (resrc_flow_t *resrc_flow)
{
    if (resrc_flow)
        return resrc_flow->children;
    return NULL;
}

int resrc_flow_add_child (resrc_flow_t *parent, resrc_flow_t *child)
{
    int rc = -1;
    if (parent) {
        child->parent = parent;
        rc = resrc_flow_list_append (parent->children, child);
    }

    return rc;
}

resrc_flow_t *resrc_flow_new (resrc_flow_t *parent, resrc_t *flow_resrc,
                              resrc_t *resrc)
{
    resrc_flow_t *resrc_flow = xzmalloc (sizeof (resrc_flow_t));

    if (resrc_flow) {
        resrc_flow->parent = parent;
        resrc_flow->resrc = resrc;
        resrc_flow->flow_resrc = flow_resrc;
        resrc_flow->children = resrc_flow_list_new ();
        if (parent)
            (void) resrc_flow_add_child (parent, resrc_flow);
    }

    return resrc_flow;
}

resrc_flow_t *resrc_flow_copy (resrc_flow_t *resrc_flow)
{
    resrc_flow_t *new_resrc_flow = xzmalloc (sizeof (resrc_flow_t));

    if (new_resrc_flow) {
        new_resrc_flow->parent = resrc_flow->parent;
        new_resrc_flow->resrc = resrc_flow->resrc;
        new_resrc_flow->flow_resrc = resrc_flow->flow_resrc;
        new_resrc_flow->children = resrc_flow_list_new ();
        new_resrc_flow->children->list = zlist_dup (resrc_flow->children->list);
    }

    return new_resrc_flow;
}

void resrc_flow_destroy (resrc_flow_t *resrc_flow)
{
    if (resrc_flow) {
        if (resrc_flow->parent)
            resrc_flow_list_remove (resrc_flow->parent->children, resrc_flow);
        if (resrc_flow->children) {
            resrc_flow_list_destroy (resrc_flow->children);
            resrc_flow->children = NULL;
        }
        resrc_resource_destroy (resrc_flow->flow_resrc);
        free (resrc_flow);
    }
}

resrc_flow_t *resrc_flow_new_from_json (json_t *o, resrc_flow_t *parent)
{
    json_t *jhierarchyo = NULL; /* json hierarchy object */
    const char *basename = NULL;
    const char *name = NULL;
    const char *hierarchy = NULL;
    const char *path = NULL;
    const char *hierarchy_path = NULL;
    const char *tmp = NULL;
    const char *type = NULL;
    int64_t id;
    int64_t ssize;
    resrc_flow_t *resrc_flow = NULL;
    resrc_t *flow_resrc;
    resrc_t *resrc = NULL;
    size_t size = 1;
    uuid_t uuid;

    if (!Jget_str (o, "type", &type))
        goto ret;
    Jget_str (o, "basename", &basename);
    Jget_str (o, "name", &name);
    if (!(Jget_int64 (o, "id", &id)))
        id = -1;
    if (Jget_str (o, "uuid", &tmp))
        uuid_parse (tmp, uuid);
    else
        uuid_clear(uuid);
    if (Jget_int64 (o, "size", &ssize))
        size = (size_t) ssize;
    if ((jhierarchyo = Jobj_get (o, "hierarchy"))) {
        /* Get first key and value from hierarchy object */
        const char *key = json_object_iter_key (json_object_iter (jhierarchyo));
        if (key) {
            hierarchy = key;
            Jget_str (jhierarchyo, key, &hierarchy_path);
        }
    }
    if (!Jget_str (o, "path", &path)) {
        if (hierarchy_path)
            path = xstrdup (hierarchy_path);
    }
    if (!path) {
        if (parent)
            path = xasprintf ("%s/%s", resrc_path (parent->flow_resrc), name);
        else
            path = xasprintf ("/%s", name);
    }

    if (!(flow_resrc = resrc_new_resource (type, path, basename, name, NULL, id,
                                           uuid, size)))
        goto ret;

    if (!strncmp (type, "node", 5)) {
        resrc = resrc_lookup (name);
    }
    if ((resrc_flow = resrc_flow_new (parent, flow_resrc, resrc))) {
        /* add time window if we are given a start time */
        int64_t starttime;
        if (Jget_int64 (o, "starttime", &starttime)) {
            int64_t endtime;
            int64_t wall_time;

            if (!Jget_int64 (o, "endtime", &endtime)) {
                if (Jget_int64 (o, "walltime", &wall_time))
                    endtime = starttime + wall_time;
                else
                    endtime = TIME_MAX;
            }

            planner_reset (resrc_twindow (resrc_flow->flow_resrc), starttime,
                endtime - starttime, NULL, 0);
        }
    }
    if (resrc)
        resrc_graph_insert (resrc, hierarchy, resrc_flow);
ret:
    free ((void*)path);
    return resrc_flow;
}


static resrc_flow_t *resrc_flow_add_rdl (resrc_flow_t *parent,
                                         struct resource *r)
{
    json_t *o = NULL;
    resrc_flow_t *resrc_flow = NULL;
    struct resource *c;

    o = rdl_resource_json (r);
    resrc_flow = resrc_flow_new_from_json (o, parent);

    while ((c = rdl_resource_next_child (r))) {
        (void) resrc_flow_add_rdl (resrc_flow, c);
        rdl_resource_destroy (c);
    }

    Jput (o);
    return resrc_flow;
}

resrc_flow_t *resrc_flow_generate_rdl (const char *path, char *uri)
{
    resrc_flow_t *flow = NULL;
    struct rdl *rdl = NULL;
    struct rdllib *l = NULL;
    struct resource *r = NULL;

    if (!(l = rdllib_open ()) || !(rdl = rdl_loadfile (l, path)))
        goto ret;

    if ((r = rdl_resource_get (rdl, uri)))
        flow = resrc_flow_add_rdl (NULL, r);

    rdl_destroy (rdl);
    rdllib_close (l);
ret:
    return flow;
}

void resrc_flow_print (resrc_flow_t *resrc_flow)
{
    if (resrc_flow) {
        resrc_print_resource (resrc_flow->flow_resrc);
        if (resrc_flow_num_children (resrc_flow)) {
            resrc_flow_t *child = resrc_flow_list_first (resrc_flow->children);
            while (child) {
                resrc_flow_print (child);
                child = resrc_flow_list_next (resrc_flow->children);
            }
        }
    }
}

int resrc_flow_serialize (json_t *o, resrc_flow_t *resrc_flow)
{
    int rc = -1;

    if (o && resrc_flow) {
        rc = resrc_to_json (o, resrc_flow->flow_resrc);
        if (!rc && resrc_flow_num_children (resrc_flow)) {
            json_t *ja = Jnew_ar ();

            if (!(rc = resrc_flow_list_serialize (ja, resrc_flow->children)))
                json_object_set_new (o, "children", ja);
        }
    }
    return rc;
}

resrc_flow_t *resrc_flow_deserialize (json_t *o, resrc_flow_t *parent)
{
    json_t *ca = NULL;     /* array of child json objects */
    json_t *co = NULL;     /* child json object */
    resrc_t *resrc = NULL;
    resrc_flow_t *resrc_flow = NULL;

    resrc = resrc_new_from_json (o, NULL, false);
    if (resrc) {
        resrc_flow = resrc_flow_new (parent, resrc, NULL);

        if ((ca = Jobj_get (o, "children"))) {
            int i, nchildren = 0;

            if (Jget_ar_len (ca, &nchildren)) {
                for (i=0; i < nchildren; i++) {
                    Jget_ar_obj (ca, i, &co);
                    (void) resrc_flow_deserialize (co, resrc_flow);
                }
            }
        }
    }

    return resrc_flow;
}

int resrc_flow_allocate (resrc_flow_t *resrc_flow, int64_t job_id,
                         int64_t starttime, int64_t endtime)
{
    int rc = -1;
    if (resrc_flow) {
        rc = resrc_allocate_resource (resrc_flow->flow_resrc, job_id,
                                      starttime, endtime);
        if (resrc_flow->parent)
            rc = resrc_flow_allocate (resrc_flow->parent, job_id,
                                      starttime, endtime);
    }
    return rc;
}

int resrc_flow_reserve (resrc_flow_t *resrc_flow, int64_t job_id,
                        int64_t starttime, int64_t endtime)
{
    int rc = -1;
    if (resrc_flow) {
        rc = resrc_reserve_resource (resrc_flow->flow_resrc, job_id,
                                     starttime, endtime);
        if (resrc_flow->parent)
            rc = resrc_flow_reserve (resrc_flow->parent, job_id,
                                     starttime, endtime);
    }
    return rc;
}

int resrc_flow_release (resrc_flow_t *resrc_flow, int64_t job_id)
{
    int rc = -1;
    if (resrc_flow) {
        rc = resrc_release_allocation (resrc_flow->flow_resrc, job_id);
        if (resrc_flow->parent)
            rc = resrc_flow_release (resrc_flow->parent, job_id);
    }
    return rc;
}

int resrc_flow_release_all_reservations (resrc_flow_t *resrc_flow)
{
    int rc = -1;
    if (resrc_flow) {
        rc = resrc_release_all_reservations (resrc_flow->flow_resrc);
        if (resrc_flow->parent)
            rc = resrc_flow_release_all_reservations (resrc_flow->parent);
    }
    return rc;
}

int resrc_flow_stage_resources (resrc_flow_t *resrc_flow, size_t size)
{
    int rc =-1;

    if (resrc_flow) {
        rc = resrc_stage_resrc (resrc_flow->flow_resrc, size, NULL);
        if (!rc && resrc_flow->parent)
            rc = resrc_flow_stage_resources (resrc_flow->parent, size);
    }
    return rc;
}

int resrc_flow_unstage_resources (resrc_flow_t *resrc_flow)
{
    int rc =-1;

    if (resrc_flow) {
        rc = resrc_unstage_resrc (resrc_flow->flow_resrc);
        if (!rc && resrc_flow->parent)
            rc = resrc_flow_unstage_resources (resrc_flow->parent);
    }
    return rc;
}

/*
 * We rely on the available value of this node in the flow graph being
 * current.  This eliminates the need to check every parent up the
 * tree for available flow.
 */
bool resrc_flow_available (resrc_flow_t *resrc_flow, size_t flow,
                           resrc_reqst_t *request)
{
    bool rc = false;

    if (resrc_flow && resrc_flow->flow_resrc) {
        resrc_t *flow_resrc = resrc_flow->flow_resrc;

        if (resrc_reqst_starttime (request))
            rc = resrc_walltime_match (flow_resrc, request, flow);
        else {
            rc = flow <= resrc_available (flow_resrc);
            if (rc && resrc_reqst_exclusive (request)) {
                rc = !resrc_size_allocs (flow_resrc) &&
                    !resrc_size_reservtns (flow_resrc);
            }
        }
    }

    return rc;
}

/***********************************************************************
 * Resource flow list
 ***********************************************************************/

resrc_flow_list_t *resrc_flow_list_new ()
{
    resrc_flow_list_t *new_list = xzmalloc (sizeof (resrc_flow_list_t));
    new_list->list = zlist_new ();
    return new_list;
}

int resrc_flow_list_append (resrc_flow_list_t *rfl, resrc_flow_t *rf)
{
    if (rfl && rfl->list && rf)
        return zlist_append (rfl->list, (void *) rf);
    return -1;
}

resrc_flow_t *resrc_flow_list_first (resrc_flow_list_t *rfl)
{
    if (rfl && rfl->list)
        return zlist_first (rfl->list);
    return NULL;
}

resrc_flow_t *resrc_flow_list_next (resrc_flow_list_t *rfl)
{
    if (rfl && rfl->list)
        return zlist_next (rfl->list);
    return NULL;
}

size_t resrc_flow_list_size (resrc_flow_list_t *rfl)
{
    if (rfl && rfl->list)
        return zlist_size (rfl->list);
    return 0;
}

void resrc_flow_list_remove (resrc_flow_list_t *rfl, resrc_flow_t *rf)
{
    zlist_remove (rfl->list, rf);
}

void resrc_flow_list_destroy (resrc_flow_list_t *resrc_flow_list)
{
    if (resrc_flow_list) {
        if (resrc_flow_list_size (resrc_flow_list)) {
            resrc_flow_t *child = resrc_flow_list_first (resrc_flow_list);
            while (child) {
                resrc_flow_destroy (child);
                child = resrc_flow_list_next (resrc_flow_list);
            }
        }
        if (resrc_flow_list->list) {
            zlist_destroy (&(resrc_flow_list->list));
        }
        free (resrc_flow_list);
    }
}

int resrc_flow_list_serialize (json_t *o, resrc_flow_list_t *rfl)
{
    resrc_flow_t *rf;
    int rc = -1;

    if (o && rfl && rfl->list) {
        rc = 0;
        rf = resrc_flow_list_first (rfl);
        while (rf) {
            json_t *co = Jnew ();

            if ((rc = resrc_flow_serialize (co, rf)))
                break;
            json_array_append_new (o, co);
            rf = resrc_flow_list_next (rfl);
        }
    }

    return rc;
}

resrc_flow_list_t *resrc_flow_list_deserialize (json_t *o)
{
    json_t *ca = NULL;     /* array of child json objects */
    int i, nchildren = 0;
    resrc_flow_t *rf = NULL;
    resrc_flow_list_t *rfl = resrc_flow_list_new ();

    if (o && rfl) {
        if (Jget_ar_len (o, &nchildren)) {
            for (i=0; i < nchildren; i++) {
                Jget_ar_obj (o, i, &ca);
                rf = resrc_flow_deserialize (ca, NULL);
                if (!rf || resrc_flow_list_append (rfl, rf))
                    break;
            }
        }
    }

    return rfl;
}

int resrc_flow_list_allocate (resrc_flow_list_t *rtl, int64_t job_id,
                              int64_t starttime, int64_t endtime)
{
    resrc_flow_t *rt;
    int rc = -1;

    if (rtl) {
        rc = 0;
        rt = resrc_flow_list_first (rtl);
        while (!rc && rt) {
            rc = resrc_flow_allocate (rt, job_id, starttime, endtime);
            rt = resrc_flow_list_next (rtl);
        }
    }

    return rc;
}

int resrc_flow_list_reserve (resrc_flow_list_t *rtl, int64_t job_id,
                             int64_t starttime, int64_t endtime)
{
    resrc_flow_t *rt;
    int rc = -1;

    if (rtl) {
        rc = 0;
        rt = resrc_flow_list_first (rtl);
        while (!rc && rt) {
            rc = resrc_flow_reserve (rt, job_id, starttime, endtime);
            rt = resrc_flow_list_next (rtl);
        }
    }

    return rc;
}

int resrc_flow_list_release (resrc_flow_list_t *rtl, int64_t job_id)
{
    resrc_flow_t *rt;
    int rc = -1;

    if (rtl) {
        rc = 0;
        rt = resrc_flow_list_first (rtl);
        while (!rc && rt) {
            rc = resrc_flow_release (rt, job_id);
            rt = resrc_flow_list_next (rtl);
        }
    }

    return rc;
}

int resrc_flow_list_release_all_reservations (resrc_flow_list_t *rtl)
{
    resrc_flow_t *rt;
    int rc = -1;

    if (rtl) {
        rc = 0;
        rt = resrc_flow_list_first (rtl);
        while (!rc && rt) {
            rc = resrc_flow_release_all_reservations (rt);
            rt = resrc_flow_list_next (rtl);
        }
    }

    return rc;
}

void resrc_flow_list_unstage_resources (resrc_flow_list_t *rtl)
{
    resrc_flow_t *rt;

    if (rtl) {
        rt = resrc_flow_list_first (rtl);
        while (rt) {
            resrc_flow_unstage_resources (rt);
            rt = resrc_flow_list_next (rtl);
        }
    }
}

/*
 * vi: ts=4 sw=4 expandtab
 */

