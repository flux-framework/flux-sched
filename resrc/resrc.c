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
#include "resrc.h"
#include "resrc_tree.h"
#include "src/common/libutil/jsonutil.h"
#include "src/common/libutil/xzmalloc.h"

struct resrc {
    char *type;
    char *path;
    char *name;
    int64_t id;
    uuid_t uuid;
    size_t size;
    size_t available;
    size_t staged;
    resource_state_t state;
    resrc_tree_t *phys_tree;
    zlist_t *graphs;
    zhash_t *properties;
    zhash_t *tags;
    zhash_t *allocs;
    zhash_t *reservtns;
    zhash_t *twindow;
};


/***************************************************************************
 *  API
 ***************************************************************************/

char *resrc_type (resrc_t *resrc)
{
    if (resrc)
        return resrc->type;
    return NULL;
}

char *resrc_path (resrc_t *resrc)
{
    if (resrc)
        return resrc->path;
    return NULL;
}

char *resrc_name (resrc_t *resrc)
{
    if (resrc)
        return resrc->name;
    return NULL;
}

int64_t resrc_id (resrc_t *resrc)
{
    if (resrc)
        return resrc->id;
    return -1;
}

size_t resrc_size (resrc_t *resrc)
{
    if (resrc)
        return resrc->size;
    return 0;
}

char* resrc_state (resrc_t *resrc)
{
    char* str = NULL;

    if (resrc) {
        switch (resrc->state) {
        case RESOURCE_INVALID:
            str = "invalid";    break;
        case RESOURCE_IDLE:
            str = "idle";       break;
        case RESOURCE_ALLOCATED:
            str = "allocated";  break;
        case RESOURCE_RESERVED:
            str = "reserved";   break;
        case RESOURCE_DOWN:
            str = "down";       break;
        case RESOURCE_UNKNOWN:
            str = "unknown";    break;
        case RESOURCE_END:
        default:
            str = "n/a";        break;
        }
    }
    return str;
}

resrc_tree_t *resrc_phys_tree (resrc_t *resrc)
{
    if (resrc)
        return resrc->phys_tree;
    return NULL;
}

resrc_t *resrc_new_resource (const char *type, const char *path,
                             const char *name, int64_t id, uuid_t uuid,
                             size_t size)
{
    resrc_t *resrc = xzmalloc (sizeof (resrc_t));
    if (resrc) {
        resrc->type = strdup (type);
        if (path)
            resrc->path = strdup (path);
        if (name)
            resrc->name = strdup (name);
        resrc->id = id;
        if (uuid)
            uuid_copy (resrc->uuid, uuid);
        resrc->size = size;
        resrc->available = size;
        resrc->staged = 0;
        resrc->state = RESOURCE_IDLE;
        resrc->phys_tree = NULL;
        resrc->graphs = NULL;
        resrc->allocs = zhash_new ();
        resrc->reservtns = zhash_new ();
        resrc->properties = zhash_new ();
        resrc->tags = zhash_new ();
        resrc->twindow = zhash_new ();
    }

    return resrc;
}

resrc_t *resrc_copy_resource (resrc_t *resrc)
{
    resrc_t *new_resrc = xzmalloc (sizeof (resrc_t));

    if (new_resrc) {
        new_resrc->type = strdup (resrc->type);
        new_resrc->path = strdup (resrc->path);
        new_resrc->name = strdup (resrc->name);
        new_resrc->id = resrc->id;
        uuid_copy (new_resrc->uuid, resrc->uuid);
        new_resrc->state = resrc->state;
        new_resrc->phys_tree = resrc_tree_copy (resrc->phys_tree);
        new_resrc->graphs = zlist_dup (resrc->graphs);
        new_resrc->allocs = zhash_dup (resrc->allocs);
        new_resrc->reservtns = zhash_dup (resrc->reservtns);
        new_resrc->properties = zhash_dup (resrc->properties);
        new_resrc->tags = zhash_dup (resrc->tags);
        new_resrc->twindow = zhash_dup (resrc->twindow);
    }

    return new_resrc;
}

void resrc_resource_destroy (void *object)
{
    resrc_t *resrc = (resrc_t *) object;

    if (resrc) {
        if (resrc->type)
            free (resrc->type);
        if (resrc->path)
            free (resrc->path);
        if (resrc->name)
            free (resrc->name);
        /* Don't worry about freeing resrc->phys_tree.  It will be
         * freed by resrc_tree_free()
         */
        if (resrc->graphs)
            zlist_destroy (&resrc->graphs);
        zhash_destroy (&resrc->allocs);
        zhash_destroy (&resrc->reservtns);
        zhash_destroy (&resrc->properties);
        zhash_destroy (&resrc->tags);
        zhash_destroy (&resrc->twindow);
        free (resrc);
    }
}

resrc_t *resrc_new_from_json (JSON o, resrc_t *parent, bool physical)
{
    JSON jhierarchyo = NULL; /* json hierarchy object */
    JSON jpropso = NULL; /* json properties object */
    JSON jtagso = NULL;  /* json tags object */
    const char *name = NULL;
    const char *path = NULL;
    const char *tmp = NULL;
    const char *type = NULL;
    int64_t jduration;
    int64_t id;
    int64_t ssize;
    json_object_iter iter;
    resrc_t *resrc = NULL;
    resrc_tree_t *parent_tree = NULL;
    size_t size = 1;
    uuid_t uuid;

    if (!Jget_str (o, "type", &type))
        goto ret;
    Jget_str (o, "name", &name);
    if (!(Jget_int64 (o, "id", &id)))
        id = 0;
    if (Jget_str (o, "uuid", &tmp))
        uuid_parse (tmp, uuid);
    else
        uuid_clear(uuid);
    if (Jget_int64 (o, "size", &ssize))
        size = (size_t) ssize;
    if (!Jget_str (o, "path", &path)) {
        if ((jhierarchyo = Jobj_get (o, "hierarchy")))
            Jget_str (jhierarchyo, "default", &path);
    }

    resrc = resrc_new_resource (type, path, name, id, uuid, size);
    if (resrc) {
        /*
         * Are we constructing the resource's physical tree?  If
         * false, this is just a resource that is part of a request.
         */
        if (physical) {
            if (parent)
                parent_tree = parent->phys_tree;
            resrc->phys_tree = resrc_tree_new (parent_tree, resrc);
        }

        jpropso = Jobj_get (o, "properties");
        if (jpropso) {
            JSON jpropo;        /* json property object */
            char *property;

            json_object_object_foreachC (jpropso, iter) {
                jpropo = Jget (iter.val);
                property = strdup (json_object_get_string (jpropo));
                zhash_insert (resrc->properties, iter.key, property);
                zhash_freefn (resrc->properties, iter.key, free);
                Jput (jpropo);
            }
        }

        jtagso = Jobj_get (o, "tags");
        if (jtagso) {
            JSON jtago;        /* json tag object */
            char *tag;

            json_object_object_foreachC (jtagso, iter) {
                jtago = Jget (iter.val);
                tag = strdup (json_object_get_string (jtago));
                zhash_insert (resrc->tags, iter.key, tag);
                zhash_freefn (resrc->tags, iter.key, free);
                Jput (jtago);
            }
        }

        /* add twindow */
        if (Jget_int64 (o, "walltime", &jduration)) {
            JSON w = Jnew ();
            Jadd_int64 (w, "walltime", jduration);
            zhash_insert (resrc->twindow, "0", (void *)Jtostr (w));
            Jput (w);
        } else if ((!strncmp (type, "node", 5)) ||
                   (!strncmp (type, "core", 5))) {
            JSON j = Jnew ();
            Jadd_int64 (j, "start", epochtime ());
            Jadd_int64 (j, "end", TIME_MAX);
            zhash_insert (resrc->twindow, "0", (void *)Jtostr (j));
            Jput (j);
        }
    }
ret:
    return resrc;
}

static resrc_t *resrc_add_resource (resrc_t *parent, struct resource *r)
{
    JSON o = NULL;
    resrc_t *resrc = NULL;
    struct resource *c;

    o = rdl_resource_json (r);
    resrc = resrc_new_from_json (o, parent, true);

    while ((c = rdl_resource_next_child (r))) {
        (void) resrc_add_resource (resrc, c);
        rdl_resource_destroy (c);
    }

    Jput (o);
    return resrc;
}

resrc_t *resrc_generate_resources (const char *path, char *resource)
{
    resrc_t *resrc = NULL;
    struct rdl *rdl = NULL;
    struct rdllib *l = NULL;
    struct resource *r = NULL;

    if (!(l = rdllib_open ()) || !(rdl = rdl_loadfile (l, path)))
        goto ret;

    if ((r = rdl_resource_get (rdl, resource)))
        resrc = resrc_add_resource (NULL, r);

    rdl_destroy (rdl);
    rdllib_close (l);
ret:
    return resrc;
}

int resrc_to_json (JSON o, resrc_t *resrc)
{
    char uuid[40];
    int rc = -1;

    if (resrc) {
        Jadd_str (o, "type", resrc_type (resrc));
        Jadd_str (o, "path", resrc_path (resrc));
        Jadd_str (o, "name", resrc_name (resrc));
        Jadd_int64 (o, "id", resrc_id (resrc));
        uuid_unparse (resrc->uuid, uuid);
        Jadd_str (o, "uuid", uuid);
        Jadd_int64 (o, "size", resrc_size (resrc));
        rc = 0;
    }
    return rc;
}

void resrc_print_resource (resrc_t *resrc)
{
    char uuid[40];
    char *property;
    char *tag;
    size_t *size_ptr;

    if (resrc) {
        uuid_unparse (resrc->uuid, uuid);
        printf ("resrc type: %s, path: %s, name: %s, id: %"PRId64", state: %s, "
                "uuid: %s, size: %"PRIu64", avail: %"PRIu64"",
                resrc->type, resrc->path, resrc->name, resrc->id,
                resrc_state (resrc), uuid, resrc->size, resrc->available);
        if (zhash_size (resrc->properties)) {
            printf (", properties:");
            property = zhash_first (resrc->properties);
            while (property) {
                printf (" %s: %s", (char *)zhash_cursor (resrc->properties),
                        property);
                property = zhash_next (resrc->properties);
            }
        }
        if (zhash_size (resrc->tags)) {
            printf (", tags:");
            tag = zhash_first (resrc->tags);
            while (tag) {
                printf (", %s", (char *)zhash_cursor (resrc->tags));
                tag = zhash_next (resrc->tags);
            }
        }
        if (zhash_size (resrc->allocs)) {
            printf (", allocs");
            size_ptr = zhash_first (resrc->allocs);
            while (size_ptr) {
                printf (", %s: %"PRIu64"",
                        (char *)zhash_cursor (resrc->allocs), *size_ptr);
                size_ptr = zhash_next (resrc->allocs);
            }
        }
        if (zhash_size (resrc->reservtns)) {
            printf (", reserved jobs");
            size_ptr = zhash_first (resrc->reservtns);
            while (size_ptr) {
                printf (", %s: %"PRIu64"",
                        (char *)zhash_cursor (resrc->reservtns), *size_ptr);
                size_ptr = zhash_next (resrc->reservtns);
            }
        }
        printf ("\n");
    }
}

/*
 * Finds if a resrc_t *sample matches with resrc_t *resrc in terms of walltime
 *
 * Note: this function is working on a resource that is already AVAILABLE.
 * Therefore it is sufficient if the walltime fits before the earliest starttime
 * of a reserved job.
 */
bool resrc_walltime_match (resrc_t *resrc, resrc_t *sample)
{
    bool rc = false;

    int64_t time_now = epochtime ();
    int64_t jstarttime;
    int64_t jendtime;
    int64_t jwalltime;
    int64_t tstarttime;
    int64_t rstarttime;
    int64_t lendtime;

    char *json_str_walltime = NULL;
    char *json_str_window = NULL;

    /* retrieve first element of twindow from request sample */
    json_str_walltime = zhash_first (sample->twindow);
    if (!json_str_walltime)
        return true;

    /* retrieve the walltime information from request sample */
    JSON jw = Jfromstr (json_str_walltime);
    if (!(Jget_int64 (jw, "walltime", &jwalltime))) {
        Jput (jw);
        return true;
    }

    /* fix job start and end time from now */
    jstarttime = time_now;
    jendtime = jstarttime + jwalltime;
    tstarttime = TIME_MAX;

    /* If jendtime is greater than the lifetime of the resource, then falst */
    json_str_window = zhash_lookup (resrc->twindow, "0");
    JSON lt = Jfromstr (json_str_window);
    Jget_int64 (lt, "endtime", &lendtime);
    Jput (lt);
    if (jendtime > (lendtime - 10)) {
        return false;
    }

    /* Iterate over resrc's twindow and find if it sample fits from now */
    json_str_window = zhash_first (resrc->twindow);
    while (json_str_window) {

        JSON rw = Jfromstr (json_str_window);
        Jget_int64 (rw, "starttime", &rstarttime);

        if (rstarttime > time_now)
            tstarttime = tstarttime < rstarttime ? tstarttime : rstarttime;

        json_str_window = zhash_next (resrc->twindow);
    }

    rc = jendtime <= tstarttime ? true : false;

    return rc;
}


bool resrc_match_resource (resrc_t *resrc, resrc_t *sample, bool available)
{
    bool rc = false;
    char *sproperty = NULL;     /* sample property */
    char *stag = NULL;          /* sample tag */

    if (!strcmp (resrc->type, sample->type) && sample->size) {
        if (sample->size > resrc->available)
            goto ret;

        if (zhash_size (sample->properties)) {
            if (!zhash_size (resrc->properties)) {
                goto ret;
            }
            /* be sure the resource has all the requested properties */
            /* TODO: validate the value of each property */
            zhash_first (sample->properties);
            do {
                sproperty = (char *)zhash_cursor (sample->properties);
                if (!zhash_lookup (resrc->properties, sproperty))
                    goto ret;
            } while (zhash_next (sample->properties));
        }

        if (zhash_size (sample->tags)) {
            if (!zhash_size (resrc->tags)) {
                goto ret;
            }
            /* be sure the resource has all the requested tags */
            zhash_first (sample->tags);
            do {
                stag = (char *)zhash_cursor (sample->tags);
                if (!zhash_lookup (resrc->tags, stag))
                    goto ret;
            } while (zhash_next (sample->tags));
        }

        if (available) {
            if ((resrc->state == RESOURCE_IDLE) && (resrc_walltime_match (resrc, sample)))
                rc = true;
        } else
            rc = true;
    }
ret:
    return rc;
}

void resrc_stage_resrc (resrc_t *resrc, size_t size)
{
    if (resrc)
        resrc->staged = size;
}

/*
 * Allocate the staged size of a resource to the specified job_id and
 * change its state to allocated.
 */
int resrc_allocate_resource (resrc_t *resrc, int64_t job_id, int64_t walltime)
{
    char *id_ptr = NULL;
    size_t *size_ptr;
    int rc = -1;
    JSON j;
    int64_t now = epochtime ();

    if (resrc && job_id) {
        if (resrc->staged > resrc->available)
            goto ret;

        id_ptr = xasprintf("%"PRId64"", job_id);
        size_ptr = xzmalloc (sizeof (size_t));
        *size_ptr = resrc->staged;
        zhash_insert (resrc->allocs, id_ptr, size_ptr);
        zhash_freefn (resrc->allocs, id_ptr, free);
        free (id_ptr);
        resrc->available -= resrc->staged;
        resrc->staged = 0;
        resrc->state = RESOURCE_ALLOCATED;

        /* add walltime */
        j = Jnew ();
        Jadd_int64 (j, "starttime", now);
        Jadd_int64 (j, "endtime", now + walltime);
        id_ptr = xasprintf ("%"PRId64"", job_id);
        zhash_insert (resrc->twindow, id_ptr, (void *)Jtostr (j));
        Jput (j);

        rc = 0;
        free (id_ptr);
    }
ret:
    return rc;
}

/*
 * Just like resrc_allocate_resource() above, but for a reservation
 */
int resrc_reserve_resource (resrc_t *resrc, int64_t job_id)
{
    char *id_ptr = NULL;
    size_t *size_ptr;
    int rc = -1;

    if (resrc && job_id) {
        if (resrc->staged > resrc->available)
            goto ret;

        id_ptr = xasprintf("%"PRId64"", job_id);
        size_ptr = xzmalloc (sizeof (size_t));
        *size_ptr = resrc->staged;
        zhash_insert (resrc->reservtns, id_ptr, size_ptr);
        zhash_freefn (resrc->reservtns, id_ptr, free);
        resrc->available -= resrc->staged;
        resrc->staged = 0;
        if (resrc->state != RESOURCE_ALLOCATED)
            resrc->state = RESOURCE_RESERVED;

        rc = 0;
        free (id_ptr);
    }
ret:
    return rc;
}

int resrc_release_resource (resrc_t *resrc, int64_t rel_job)
{
    char *id_ptr = NULL;
    size_t *size_ptr;
    int rc = 0;

    if (!resrc || !rel_job) {
        rc = -1;
        goto ret;
    }

    id_ptr = xasprintf ("%"PRId64"", rel_job);
    size_ptr = zhash_lookup (resrc->allocs, id_ptr);
    if (size_ptr) {
        resrc->available += *size_ptr;
        zhash_delete (resrc->allocs, id_ptr);
        zhash_delete (resrc->twindow, id_ptr);
        if (!zhash_size (resrc->allocs)) {
            if (zhash_size (resrc->reservtns))
                resrc->state = RESOURCE_RESERVED;
            else
                resrc->state = RESOURCE_IDLE;
        }
    }
    free (id_ptr);
ret:
    return rc;
}


/*
 * vi: ts=4 sw=4 expandtab
 */

