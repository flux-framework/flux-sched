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

typedef struct {
    int64_t job_id;
    int64_t items;
} pool_alloc_t;

typedef struct {
    char *type;
    int64_t avail_items;
    int64_t total_items;
    zlist_t *allocs;
} resrc_pool_t;

struct resrc {
    char *type;
    char *name;
    int64_t id;
    int32_t max_jobs;
    uuid_t uuid;
    resource_state_t state;
    resrc_tree_t *phys_tree;
    zlist_t *graphs;
    zlist_t *jobs;
    zlist_t *resrv_jobs;
    zlist_t *pools;
    zlist_t *properties;
    zlist_t *tags;
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

resrc_tree_t *resrc_phys_tree (resrc_t *resrc)
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

resource_list_t *resrc_new_id_list ()
{
    return (resource_list_t *) zlist_new ();
}

void resrc_id_list_destroy (resource_list_t *resrc_ids_in)
{
    zlist_t * resrc_ids = (zlist_t*)resrc_ids_in;
    char *resrc_id;

    if (resrc_ids) {
        while ((resrc_id = zlist_pop (resrc_ids)))
            free (resrc_id);
        zlist_destroy (&resrc_ids);
    }
}

char *resrc_list_first (resource_list_t *rl)
{
    return zlist_first ((zlist_t*)rl);
}

char *resrc_list_next (resource_list_t *rl)
{
    return zlist_next ((zlist_t*)rl);
}

size_t resrc_list_size (resource_list_t *rl)
{
    return zlist_size ((zlist_t*)rl);
}

resrc_pool_t *resrc_new_pool (const char *type, int64_t items)
{
    resrc_pool_t *pool = xzmalloc (sizeof (resrc_pool_t));

    if (pool) {
        pool->type = strdup (type);
        pool->total_items = items;
        pool->avail_items = items;
        pool->allocs = zlist_new ();
    } else {
        oom ();
    }

    return pool;
}

void resrc_pool_destroy (resrc_pool_t *pool)
{
    pool_alloc_t *alloc;

    if (pool) {
        while ((alloc = zlist_pop (pool->allocs)))
            free (alloc);
        zlist_destroy (&pool->allocs);
        free (pool);
    }
}

void resrc_pool_list_destroy (zlist_t *pools)
{
    resrc_pool_t *pool;

    if (pools) {
        while ((pool = zlist_pop (pools))) {
            free (pool->type);
            resrc_pool_destroy (pool);
        }
        zlist_destroy (&pools);
    }
}

resrc_t *resrc_new_resource (const char *type, const char *name, int64_t id,
                                uuid_t uuid)
{
    resrc_t *resrc = xzmalloc (sizeof (resrc_t));
    if (resrc) {
        resrc->type = strdup (type);
        if (name)
            resrc->name = strdup (name);
        resrc->id = id;
        resrc->max_jobs = 1;
        if (uuid)
            uuid_copy (resrc->uuid, uuid);
        resrc->state = RESOURCE_INVALID;
        resrc->phys_tree = NULL;
        resrc->graphs = NULL;
        resrc->jobs = zlist_new ();
        resrc->resrv_jobs = zlist_new ();
        resrc->pools = zlist_new ();
        resrc->properties = NULL;
        resrc->tags = zlist_new ();
    } else {
        oom ();
    }

    return resrc;
}

resrc_t *resrc_copy_resource (resrc_t *resrc)
{
    resrc_t *new_resrc = xzmalloc (sizeof (resrc_t));

    if (new_resrc) {
        new_resrc->type = strdup (resrc->type);
        new_resrc->name = strdup (resrc->name);
        new_resrc->id = resrc->id;
        new_resrc->max_jobs = resrc->max_jobs;
        uuid_copy (new_resrc->uuid, resrc->uuid);
        new_resrc->state = resrc->state;
        new_resrc->phys_tree = resrc_tree_copy (resrc->phys_tree);
        new_resrc->graphs = zlist_dup (resrc->graphs);
        new_resrc->jobs = zlist_dup (resrc->jobs);
        new_resrc->resrv_jobs = zlist_dup (resrc->resrv_jobs);
        new_resrc->pools = zlist_dup (resrc->pools);
        new_resrc->properties = zlist_dup (resrc->properties);
        new_resrc->tags = zlist_dup (resrc->tags);
    } else {
        oom ();
    }

    return new_resrc;
}

void resrc_resource_destroy (void *object)
{
    char *tmp;
    int64_t *id_ptr;
    resrc_t *resrc = (resrc_t *) object;

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
        while ((id_ptr = zlist_pop (resrc->jobs)))
            jobid_destroy (id_ptr);
        zlist_destroy (&resrc->jobs);
        while ((id_ptr = zlist_pop (resrc->resrv_jobs)))
            jobid_destroy (id_ptr);
        zlist_destroy (&resrc->resrv_jobs);
        if (resrc->pools)
            resrc_pool_list_destroy (resrc->pools);
        if (resrc->properties)
            zlist_destroy (&resrc->properties);
        while ((tmp = zlist_pop (resrc->tags)))
            free (tmp);
        zlist_destroy (&resrc->tags);
        free (resrc);
    }
}

static resrc_t *resrc_add_resource (zhash_t *resrcs, resrc_t *parent,
                                    struct resource *r)
{
    char *fullname = NULL;
    char *nodename = NULL;
    const char *name = NULL;
    const char *tmp = NULL;
    const char *type = NULL;
    int64_t id;
    JSON o = NULL;
    JSON jtago = NULL;  /* json tag object */
    json_object_iter iter;
    resrc_t *resrc = NULL;
    resrc_tree_t *parent_tree = NULL;
    resrc_tree_t *resrc_tree = NULL;
    struct resource *c;
    uuid_t uuid;

    o = rdl_resource_json (r);
    Jget_str (o, "type", &type);
    Jget_str (o, "name", &name);
    jtago = Jobj_get (o, "tags");
    Jget_str (o, "uuid", &tmp);
    uuid_parse (tmp, uuid);
    if (!(Jget_int64 (o, "id", &id)))
        id = 0;

    /*
     * If we are running within a SLURM allocation, ignore any rdl
     * node resources that are not part of the allocation.
     */
    if (slurm_job && !strncmp (type, "node", 5) && id) {
        asprintf (&nodename, "%s%ld", name, id);
        if (!hostset_within(hostset, nodename))
            goto ret;
    }

    if (parent) {
        asprintf (&fullname, "%s.%s.%ld", parent->name, name, id);
        parent_tree = parent->phys_tree;
        if (!strncmp (type, "memory", 6)) {
            int64_t items;
            resrc_pool_t *pool;

            Jget_int64 (o, "size", &items);
            pool = resrc_new_pool (type, items);
            if (pool) {
                zlist_append (parent->pools, pool);
                goto ret;
            } else {
                oom ();
            }
        }
    } else {
        asprintf (&fullname, "%s.%ld", name, id);
    }
    resrc = resrc_new_resource (type, fullname, id, uuid);

    if (resrc) {
        resrc->state = RESOURCE_IDLE;
        resrc_tree = resrc_tree_new (parent_tree, resrc);
        resrc->phys_tree = resrc_tree;
        zhash_insert (resrcs, fullname, resrc);
        zhash_freefn (resrcs, fullname, resrc_resource_destroy);
        if (jtago) {
            json_object_object_foreachC (jtago, iter) {
                zlist_append (resrc->tags, strdup (iter.key));
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
    free (fullname);
    free (nodename);
    return resrc;
}

resources_t *resrc_generate_resources (const char *path, char *resource)
{
    const char *nodelist = NULL;
    resrc_t *resrc = NULL;
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

    if ((nodelist = getenv ("SLURM_JOB_NODELIST"))) {
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
    return (resources_t *)resrcs;
}

void resrc_destroy_resources (resources_t **resources)
{
    zhash_destroy ((zhash_t**)resources);
}

int resrc_to_json (JSON o, resrc_t *resrc)
{
    int rc = -1;
    if (resrc) {
        Jadd_str (o, resrc_name (resrc), resrc_type (resrc));
        rc = 0;
    }
    return rc;
}

void resrc_print_resource (resrc_t *resrc)
{
    char out[40];
    char *tag;
    int64_t *id_ptr;
    resrc_pool_t *pool_ptr;

    if (resrc) {
        uuid_unparse (resrc->uuid, out);
        printf ("resrc type:%s, name:%s, id:%ld, state:%d, uuid: %s",
                resrc->type, resrc->name, resrc->id, resrc->state, out);
        if (zlist_size (resrc->tags)) {
            printf (", tags");
            tag = zlist_first (resrc->tags);
            while (tag) {
                printf (", %s", tag);
                tag = zlist_next (resrc->tags);
            }
        }
        if (zlist_size (resrc->pools)) {
            printf (", pools");
            pool_ptr = zlist_first (resrc->pools);
            while (pool_ptr) {
                printf (", %s: %ld of %ld", pool_ptr->type,
                        pool_ptr->avail_items, pool_ptr->total_items);
                pool_ptr = zlist_next (resrc->pools);
            }
        }
        if (zlist_size (resrc->jobs)) {
            printf (", jobs");
            id_ptr = zlist_first (resrc->jobs);
            while (id_ptr) {
                printf (", %ld", *id_ptr);
                id_ptr = zlist_next (resrc->jobs);
            }
        }
        if (zlist_size(resrc->resrv_jobs)) {
            printf (", reserved jobs");
            id_ptr = zlist_first (resrc->resrv_jobs);
            while (id_ptr) {
                printf (", %ld", *id_ptr);
                id_ptr = zlist_next (resrc->resrv_jobs);
            }
        }
        printf ("\n");
    }
}

void resrc_print_resources (resources_t *resrcs)
{
    char *resrc_id = NULL;
    resrc_t *resrc = NULL;
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
    resrc_id_list_destroy ((resource_list_t *)resrc_ids);
}

bool resrc_match_resource (resrc_t *resrc, resrc_t *sample, bool available)
{
    bool rc = false;
    bool match = false;
    char *rtag = NULL;
    char *stag = NULL;

    if (!strncmp (resrc->type, sample->type, sizeof (sample->type))) {
        if (zlist_size (sample->tags)) {
            if (!zlist_size (resrc->tags)) {
                goto ret;
            }
            stag = zlist_first (sample->tags);
            while (stag) {
                rtag = zlist_first (resrc->tags);
                match = false;
                while (rtag) {
                    if (!strcmp (rtag, stag))
                        match = true;
                    rtag = zlist_next (resrc->tags);
                }
                if (!match)
                    goto ret;
                stag = zlist_next (sample->tags);
            }
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
int resrc_search_flat_resources (resources_t *resrcs_in,
                                 resource_list_t *found_in, JSON req_res,
                                 bool available)
{
    zhash_t * resrcs = (zhash_t*)resrcs_in;
    zlist_t * found = (zlist_t*)found_in;
    char *resrc_id = NULL;
    const char *type = NULL;
    int nfound = 0;
    int req_qty = 0;
    resrc_t *resrc;
    zlist_t *resrc_ids;

    if (!resrcs || !found || !req_res) {
        goto ret;
    }

    Jget_str (req_res, "type", &type);
    Jget_int (req_res, "req_qty", &req_qty);

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

    resrc_id_list_destroy ((resource_list_t *)resrc_ids);
ret:
    return nfound;
}
#endif

int resrc_allocate_resource (resrc_t *resrc, int64_t job_id)
{
    int64_t *id_ptr;
    int rc = -1;

    if (resrc && job_id) {
        id_ptr = xzmalloc (sizeof (int64_t));
        *id_ptr = job_id;
        zlist_append (resrc->jobs, id_ptr);
        resrc->state = RESOURCE_ALLOCATED;
        rc = 0;
    }

    return rc;
}

int resrc_allocate_resources (resources_t *resrcs_in,
                              resource_list_t *resrc_ids_in, int64_t job_id)
{
    zhash_t * resrcs = (zhash_t *)resrcs_in;
    zlist_t * resrc_ids = (zlist_t*)resrc_ids_in;
    char *resrc_id;
    resrc_t *resrc;
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

int resrc_reserve_resource (resrc_t *resrc, int64_t job_id)
{
    int64_t *id_ptr;
    int rc = -1;

    if (resrc && job_id) {
        id_ptr = xzmalloc (sizeof (int64_t));
        *id_ptr = job_id;
        zlist_append (resrc->resrv_jobs, id_ptr);
        if (resrc->state != RESOURCE_ALLOCATED)
            resrc->state = RESOURCE_RESERVED;
        rc = 0;
    }

    return rc;
}

int resrc_reserve_resources (resources_t *resrcs_in,
                             resource_list_t *resrc_ids_in, int64_t job_id)
{
    zhash_t * resrcs = (zhash_t*)resrcs_in;
    zlist_t * resrc_ids = (zlist_t*)resrc_ids_in;
    char *resrc_id;
    resrc_t *resrc;
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

JSON resrc_id_serialize (resources_t *resrcs_in, resource_list_t *resrc_ids_in)
{
    zhash_t * resrcs = (zhash_t *)resrcs_in;
    zlist_t * resrc_ids = (zlist_t*)resrc_ids_in;
    char *resrc_id;
    JSON ja;
    JSON o = NULL;
    resrc_t *resrc;

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
    Jadd_obj (o, "resrcs", ja);
ret:
    return o;
}

int resrc_release_resource (resrc_t *resrc, int64_t rel_job)
{
    int64_t *id_ptr;
    int rc = 0;

    if (!resrc || !rel_job) {
        rc = -1;
        goto ret;
    }

    id_ptr = zlist_first (resrc->jobs);
    while (id_ptr) {
        if (*id_ptr == rel_job) {
            zlist_remove (resrc->jobs, id_ptr);
            jobid_destroy (id_ptr);
            break;
        }
        id_ptr = zlist_next (resrc->jobs);
    }
    if (!zlist_size (resrc->jobs)) {
        if (zlist_size (resrc->resrv_jobs))
            resrc->state = RESOURCE_RESERVED;
        else
            resrc->state = RESOURCE_IDLE;
    }
ret:
    return rc;
}

int resrc_release_resources (resources_t *resrcs_in,
                             resource_list_t * resrc_ids_in, int64_t rel_job)
{
    zhash_t * resrcs = (zhash_t *)resrcs_in;
    zlist_t * resrc_ids = (zlist_t*)resrc_ids_in;
    char *resrc_id;
    resrc_t *resrc;
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

resrc_t *resrc_new_from_json (JSON o, resrc_t *parent)
{
    const char *type = NULL;
    int qty = 0;
    JSON ca = NULL;     /* array of child json objects */
    JSON co = NULL;     /* child json object */
    JSON jtago = NULL;  /* json tag object */
    json_object_iter iter;
    resrc_t *resrc = NULL;
    resrc_tree_t *parent_tree = NULL;
    resrc_tree_t *resrc_tree = NULL;

    if (!Jget_str (o, "type", &type))
        goto ret;

    if (parent) {
        parent_tree = resrc_phys_tree (parent);
        if (!strncmp (type, "memory", 6)) {
            int64_t items;
            resrc_pool_t *pool;

            if (Jget_int64 (o, "size", &items)) {
                pool = resrc_new_pool (type, items);
                if (pool) {
                    zlist_append (parent->pools, pool);
                    goto ret;
                } else {
                    oom ();
                }
            } else {
                printf ("missing size for memory resource\n");
            }
        }
    }

    resrc = resrc_new_resource (type, NULL, -1, 0);
    if (resrc) {
        resrc->state = RESOURCE_INVALID;
        resrc_tree = resrc_tree_new (parent_tree, resrc);
        resrc->phys_tree = resrc_tree;

        jtago = Jobj_get (o, "tags");
        if (jtago) {
            json_object_object_foreachC (jtago, iter) {
                zlist_append (resrc->tags, strdup (iter.key));
            }
        }

        if ((co = Jobj_get (o, "req_child"))) {
            if (Jget_int (co, "req_qty", &qty) && (qty > 0)) {
                while (qty--) {
                    (void) resrc_new_from_json (co, resrc);
                }
            }
        } else if ((ca = Jobj_get (o, "req_children"))) {
            int i, nchildren = 0;

            if (Jget_ar_len (ca, &nchildren)) {
                for (i=0; i < nchildren; i++) {
                    Jget_ar_obj (ca, i, &co);
                    if (Jget_int (co, "req_qty", &qty) && (qty > 0)) {
                        while (qty--) {
                            (void) resrc_new_from_json (co, resrc);
                        }
                    }
                }
            }
        }
    }
ret:
    return resrc;
}


/*
 * vi: ts=4 sw=4 expandtab
 */

