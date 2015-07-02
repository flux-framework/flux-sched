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
#include "src/common/liblsd/hostlist.h"
#include "src/common/libutil/jsonutil.h"
#include "src/common/libutil/xzmalloc.h"

static bool slurm_job = false;
static hostset_t hostset = NULL;

typedef struct zhash_t resources;
typedef struct zlist_t resource_list;

struct resrc {
    char *type;
    char *name;
    int64_t id;
    uuid_t uuid;
    size_t size;
    size_t available;
    size_t staged;
    resource_state_t state;
    resrc_tree_t phys_tree;
    zlist_t *graphs;
    zhash_t *properties;
    zhash_t *tags;
    zhash_t *allocs;
    zhash_t *reservtns;
};


/***************************************************************************
 *  API
 ***************************************************************************/

char *resrc_type (resrc_t resrc)
{
    if (resrc)
        return resrc->type;
    return NULL;
}

char *resrc_name (resrc_t resrc)
{
    if (resrc)
        return resrc->name;
    return NULL;
}

int64_t resrc_id (resrc_t resrc)
{
    if (resrc)
        return resrc->id;
    return -1;
}

size_t resrc_size (resrc_t resrc)
{
    if (resrc)
        return resrc->size;
    return 0;
}

char* resrc_state (resrc_t resrc)
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

resrc_tree_t resrc_phys_tree (resrc_t resrc)
{
    if (resrc)
        return resrc->phys_tree;
    return NULL;
}

void jobid_destroy (void *object)
{
    int64_t *tmp = (int64_t *)object;
    free (tmp);
}

resource_list_t resrc_new_id_list ()
{
    return (resource_list_t) zlist_new ();
}

void resrc_id_list_destroy (resource_list_t resrc_ids_in)
{
    zlist_t *resrc_ids = (zlist_t*)resrc_ids_in;
    char *resrc_id;

    if (resrc_ids) {
        while ((resrc_id = zlist_pop (resrc_ids)))
            free (resrc_id);
        zlist_destroy (&resrc_ids);
    }
}

char *resrc_list_first (resource_list_t rl)
{
    return zlist_first ((zlist_t*)rl);
}

char *resrc_list_next (resource_list_t rl)
{
    return zlist_next ((zlist_t*)rl);
}

size_t resrc_list_size (resource_list_t rl)
{
    return zlist_size ((zlist_t*)rl);
}

resrc_t resrc_new_resource (const char *type, const char *name, int64_t id,
                            uuid_t uuid, size_t size)
{
    resrc_t resrc = xzmalloc (sizeof (struct resrc));
    if (resrc) {
        resrc->type = strdup (type);
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
    } else {
        oom ();
    }

    return resrc;
}

resrc_t resrc_copy_resource (resrc_t resrc)
{
    resrc_t new_resrc = xzmalloc (sizeof (struct resrc));

    if (new_resrc) {
        new_resrc->type = strdup (resrc->type);
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
    } else {
        oom ();
    }

    return new_resrc;
}

void resrc_resource_destroy (void *object)
{
    resrc_t resrc = (resrc_t) object;

    if (resrc) {
        if (resrc->type)
            free (resrc->type);
        if (resrc->name)
            free (resrc->name);
        if (resrc->phys_tree) {
            resrc_tree_free (resrc->phys_tree);
            resrc->phys_tree = NULL;
        }
        if (resrc->graphs)
            zlist_destroy (&resrc->graphs);
        zhash_destroy (&resrc->allocs);
        zhash_destroy (&resrc->reservtns);
        zhash_destroy (&resrc->properties);
        zhash_destroy (&resrc->tags);
        free (resrc);
    }
}

static resrc_t resrc_add_resource (zhash_t *resrcs, resrc_t parent,
                                   struct resource *r)
{
    const char *name = NULL;
    const char *path = NULL;
    const char *tmp = NULL;
    const char *type = NULL;
    int64_t id;
    size_t size;
    JSON o = NULL;
    JSON jtago = NULL;  /* json tag object */
    json_object_iter iter;
    resrc_t resrc = NULL;
    resrc_tree_t parent_tree = NULL;
    resrc_tree_t resrc_tree = NULL;
    struct resource *c;
    uuid_t uuid;

    o = rdl_resource_json (r);
    Jget_str (o, "type", &type);
    if (!(Jget_int64 (o, "id", &id)))
        id = 0;
    Jget_str (o, "uuid", &tmp);
    uuid_parse (tmp, uuid);
    name = rdl_resource_name(r);    /* includes the id */
    path = rdl_resource_path(r);
    size = rdl_resource_size(r);
    jtago = Jobj_get (o, "tags");

    /*
     * If we are running within a SLURM allocation, ignore any rdl
     * node resources that are not part of the allocation.
     */
    if (slurm_job && !strncmp (type, "node", 5)) {
        if (!hostset_within(hostset, name))
            goto ret;
    }

    if (parent)
        parent_tree = parent->phys_tree;
    resrc = resrc_new_resource (type, name, id, uuid, size);

    if (resrc) {
        resrc_tree = resrc_tree_new (parent_tree, resrc);
        resrc->phys_tree = resrc_tree;
        zhash_insert (resrcs, path, resrc);
        zhash_freefn (resrcs, path, resrc_resource_destroy);
        if (jtago) {
            json_object_object_foreachC (jtago, iter) {
                zhash_insert (resrc->tags, iter.key, "t");
            }
        }

        while ((c = rdl_resource_next_child (r))) {
            (void) resrc_add_resource (resrcs, resrc, c);
            rdl_resource_destroy (c);
        }
    } else {
        oom ();
    }
ret:
    Jput (o);
    return resrc;
}

resources_t resrc_generate_resources (const char *path, char *resource)
{
    const char *nodelist = NULL;
    resrc_t resrc = NULL;
    struct rdl *rdl = NULL;
    struct rdllib *l = NULL;
    struct resource *r = NULL;
    zhash_t *resrcs = NULL;

    if (!(l = rdllib_open ()) || !(rdl = rdl_loadfile (l, path)))
        goto ret;

    if (!(r = rdl_resource_get (rdl, resource)))
        goto ret;

    if (!(resrcs = zhash_new ()))
        goto ret;

    if ((nodelist = getenv ("SLURM_NODELIST"))) {
        hostset = hostset_create (nodelist);
        if (hostset)
            slurm_job = true;
    }

    if (!(resrc = resrc_add_resource (resrcs, NULL, r)))
        goto ret;

    /* add the root resource to the table under a unique key */
    zhash_insert (resrcs, "head", resrc);

    rdl_destroy (rdl);
    rdllib_close (l);
ret:
    return (resources_t)resrcs;
}

void resrc_destroy_resources (resources_t *resources)
{
    zhash_destroy ((zhash_t**)resources);
}

int resrc_to_json (JSON o, resrc_t resrc)
{
    int rc = -1;
    if (resrc) {
        Jadd_str (o, resrc_name (resrc), resrc_type (resrc));
        rc = 0;
    }
    return rc;
}

void resrc_print_resource (resrc_t resrc)
{
    char uuid[40];
    char *tag;
    size_t *size_ptr;

    if (resrc) {
        uuid_unparse (resrc->uuid, uuid);
        printf ("resrc type:%s, name:%s, id:%ld, state: %s, uuid: %s, size: %lu,"
                " avail: %lu",
                resrc->type, resrc->name, resrc->id, resrc_state (resrc), uuid,
                resrc->size, resrc->available);
        if (zhash_size (resrc->tags)) {
            printf (", tags");
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
                printf (", %s: %lu",
                        (char *)zhash_cursor (resrc->allocs), *size_ptr);
                size_ptr = zhash_next (resrc->allocs);
            }
        }
        if (zhash_size (resrc->reservtns)) {
            printf (", reserved jobs");
            size_ptr = zhash_first (resrc->reservtns);
            while (size_ptr) {
                printf (", %s: %lu",
                        (char *)zhash_cursor (resrc->reservtns), *size_ptr);
                size_ptr = zhash_next (resrc->reservtns);
            }
        }
        printf ("\n");
    }
}

void resrc_print_resources (resources_t resrcs)
{
    char *resrc_id = NULL;
    resrc_t resrc = NULL;
    zlist_t *resrc_ids = NULL;

    if (!resrcs) {
        return;
    }

    resrc_ids = zhash_keys ((zhash_t*)resrcs);
    resrc_id = zlist_first (resrc_ids);
    while (resrc_id) {
        resrc = zhash_lookup ((zhash_t*)resrcs, resrc_id);
        resrc_print_resource (resrc);
        resrc_id = zlist_next (resrc_ids);
    }
    resrc_id_list_destroy ((resource_list_t)resrc_ids);
}

bool resrc_match_resource (resrc_t resrc, resrc_t sample, bool available)
{
    bool rc = false;
    char *stag = NULL;          /* sample tag */

    if (!strcmp (resrc->type, sample->type) && sample->size) {
        if (sample->size > resrc->available)
            goto ret;

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
            if (resrc->state == RESOURCE_IDLE)
                rc = true;
        } else
            rc = true;
    }
ret:
    return rc;
}

#if 0
int resrc_search_flat_resources (resources_t resrcs_in,
                                 resource_list_t found_in, JSON req_res,
                                 bool available)
{
    zhash_t *resrcs = (zhash_t*)resrcs_in;
    zlist_t *found = (zlist_t*)found_in;
    char *resrc_id = NULL;
    const char *type = NULL;
    int nfound = 0;
    resrc_t resrc;
    zlist_t *resrc_ids;

    if (!resrcs || !found || !req_res) {
        goto ret;
    }

    Jget_str (req_res, "type", &type);
    resrc_ids = zhash_keys (resrcs);
    resrc_id = zlist_first (resrc_ids);
    while (resrc_id) {
        resrc = zhash_lookup (resrcs, resrc_id);
        if (resrc_match_resource (resrc, type, available)) {
            zlist_append (found, strdup (resrc_id));
            nfound++;
        }
        resrc_id = zlist_next (resrc_ids);
    }

    resrc_id_list_destroy ((resource_list_t)resrc_ids);
ret:
    return nfound;
}
#endif

void resrc_stage_resrc (resrc_t resrc, size_t size)
{
    if (resrc)
        resrc->staged = size;
}

/*
 * Allocate the staged size of a resource to the specified job_id and
 * change its state to allocated.
 */
int resrc_allocate_resource (resrc_t resrc, int64_t job_id)
{
    char *id_ptr = NULL;
    size_t *size_ptr;
    int rc = -1;

    if (resrc && job_id) {
        if (resrc->staged > resrc->available)
            goto ret;

        asprintf(&id_ptr, "%ld", job_id);
        size_ptr = xzmalloc (sizeof (size_t));
        *size_ptr = resrc->staged;
        zhash_insert (resrc->allocs, id_ptr, size_ptr);
        resrc->available -= resrc->staged;
        resrc->staged = 0;
        resrc->state = RESOURCE_ALLOCATED;
        rc = 0;
    }
ret:
    return rc;
}

int resrc_allocate_resources (resources_t resrcs_in,
                              resource_list_t resrc_ids_in, int64_t job_id)
{
    zhash_t *resrcs = (zhash_t *)resrcs_in;
    zlist_t *resrc_ids = (zlist_t*)resrc_ids_in;
    char *resrc_id;
    resrc_t resrc;
    int rc = 0;

    if (!resrcs || !resrc_ids || !job_id) {
        rc = -1;
        goto ret;
    }

    resrc_id = zlist_first (resrc_ids);
    while (!rc && resrc_id) {
        resrc = zhash_lookup (resrcs, resrc_id);
        rc = resrc_allocate_resource (resrc, job_id);
        resrc_id = zlist_next (resrc_ids);
    }
ret:
    return rc;
}

/*
 * Just like resrc_allocate_resource() above, but for a reservation
 */
int resrc_reserve_resource (resrc_t resrc, int64_t job_id)
{
    char *id_ptr = NULL;
    size_t *size_ptr;
    int rc = -1;

    if (resrc && job_id) {
        if (resrc->staged > resrc->available)
            goto ret;

        asprintf(&id_ptr, "%ld", job_id);
        size_ptr = xzmalloc (sizeof (size_t));
        *size_ptr = resrc->staged;
        zhash_insert (resrc->reservtns, id_ptr, size_ptr);
        resrc->available -= resrc->staged;
        resrc->staged = 0;
        if (resrc->state != RESOURCE_ALLOCATED)
            resrc->state = RESOURCE_RESERVED;

        rc = 0;
    }
ret:
    return rc;
}

int resrc_reserve_resources (resources_t resrcs_in,
                             resource_list_t resrc_ids_in, int64_t job_id)
{
    zhash_t *resrcs = (zhash_t*)resrcs_in;
    zlist_t *resrc_ids = (zlist_t*)resrc_ids_in;
    char *resrc_id;
    resrc_t resrc;
    int rc = 0;

    if (!resrcs || !resrc_ids || !job_id) {
        rc = -1;
        goto ret;
    }

    resrc_id = zlist_first (resrc_ids);
    while (!rc && resrc_id) {
        resrc = zhash_lookup (resrcs, resrc_id);
        rc = resrc_reserve_resource (resrc, job_id);
        resrc_id = zlist_next (resrc_ids);
    }
ret:
    return rc;
}

JSON resrc_id_serialize (resources_t resrcs_in, resource_list_t resrc_ids_in)
{
    zhash_t *resrcs = (zhash_t *)resrcs_in;
    zlist_t *resrc_ids = (zlist_t*)resrc_ids_in;
    char *resrc_id;
    JSON ja;
    JSON o = NULL;
    resrc_t resrc;

    if (!resrcs || !resrc_ids) {
        goto ret;
    }

    o = Jnew ();
    ja = Jnew_ar ();
    resrc_id = zlist_first (resrc_ids);
    while (resrc_id) {
        resrc = zhash_lookup (resrcs, resrc_id);
        Jadd_ar_str (ja, resrc->name);
        resrc_id = zlist_next (resrc_ids);
    }
    json_object_object_add (o, "resrcs", ja);
ret:
    return o;
}

int resrc_release_resource (resrc_t resrc, int64_t rel_job)
{
    char *id_ptr = NULL;
    size_t *size_ptr;
    int rc = 0;

    if (!resrc || !rel_job) {
        rc = -1;
        goto ret;
    }

    asprintf(&id_ptr, "%ld", rel_job);
    size_ptr = zhash_lookup (resrc->allocs, id_ptr);
    if (size_ptr) {
        resrc->available += *size_ptr;
        zhash_delete (resrc->allocs, id_ptr);
        if (!zhash_size (resrc->allocs)) {
            if (zhash_size (resrc->reservtns))
                resrc->state = RESOURCE_RESERVED;
            else
                resrc->state = RESOURCE_IDLE;
        }
    }
ret:
    return rc;
}

int resrc_release_resources (resources_t resrcs_in,
                             resource_list_t resrc_ids_in, int64_t rel_job)
{
    zhash_t *resrcs = (zhash_t *)resrcs_in;
    zlist_t *resrc_ids = (zlist_t*)resrc_ids_in;
    char *resrc_id;
    resrc_t resrc;
    int rc = 0;

    if (!resrcs || !resrc_ids || !rel_job) {
        rc = -1;
        goto ret;
    }

    resrc_id = zlist_first (resrc_ids);
    while (!rc && resrc_id) {
        resrc = zhash_lookup (resrcs, resrc_id);
        rc = resrc_release_resource (resrc, rel_job);
        resrc_id = zlist_next (resrc_ids);
    }
ret:
    return rc;
}

resrc_t resrc_new_from_json (JSON o, resrc_t parent)
{
    JSON jtago = NULL;  /* json tag object */
    const char *type = NULL;
    int64_t ssize;
    json_object_iter iter;
    resrc_t resrc = NULL;
    size_t size = 1;

    if (!Jget_str (o, "type", &type))
        goto ret;
    if (Jget_int64 (o, "size", &ssize))
        size = (size_t) ssize;

    resrc = resrc_new_resource (type, NULL, -1, 0, size);
    if (resrc) {
        jtago = Jobj_get (o, "tags");
        if (jtago) {
            json_object_object_foreachC (jtago, iter) {
                zhash_insert (resrc->tags, iter.key, "t");
            }
        }
    }
ret:
    return resrc;
}


/*
 * vi: ts=4 sw=4 expandtab
 */

