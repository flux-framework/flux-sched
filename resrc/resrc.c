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
    int64_t avail_items;
    int64_t selctd_items;
    int64_t total_items;
    zhash_t *allocs;
    zhash_t *reservtns;
} resrc_pool_t;

struct resrc {
    char *type;
    char *name;
    int64_t id;
    int32_t max_jobs;
    uuid_t uuid;
    resource_state_t state;
    resrc_tree_t phys_tree;
    zlist_t *graphs;
    zlist_t *jobs;
    zlist_t *resrv_jobs;
    zhash_t *pools;
    zlist_t *properties;
    zhash_t *tags;
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

static void free_int64 (void **pval)
{
    int64_t *int64p = *pval;
    free (int64p);
}

static resrc_pool_t *resrc_new_pool (int64_t items)
{
    resrc_pool_t *pool = xzmalloc (sizeof (resrc_pool_t));

    if (pool) {
        pool->total_items = items;
        pool->avail_items = items;
        pool->selctd_items = 0;
        pool->allocs = zhash_new ();
        zhash_set_destructor (pool->allocs, free_int64);
        pool->reservtns = zhash_new ();
        zhash_set_destructor (pool->reservtns, free_int64);
    } else {
        oom ();
    }

    return pool;
}

static void resrc_pool_hash_destroy (void **pval)
{
    resrc_pool_t *pool = *pval;

    if (pool) {
        zhash_destroy (&pool->allocs);
        zhash_destroy (&pool->reservtns);
	free (pool);
    }
}

resrc_t resrc_new_resource (const char *type, const char *name, int64_t id,
                            uuid_t uuid)
{
    resrc_t resrc = xzmalloc (sizeof (struct resrc));
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
        resrc->pools = zhash_new ();
        zhash_set_destructor (resrc->pools, resrc_pool_hash_destroy);
        resrc->properties = NULL;
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
        new_resrc->max_jobs = resrc->max_jobs;
        uuid_copy (new_resrc->uuid, resrc->uuid);
        new_resrc->state = resrc->state;
        new_resrc->phys_tree = resrc_tree_copy (resrc->phys_tree);
        new_resrc->graphs = zlist_dup (resrc->graphs);
        new_resrc->jobs = zlist_dup (resrc->jobs);
        new_resrc->resrv_jobs = zlist_dup (resrc->resrv_jobs);
        new_resrc->pools = zhash_dup (resrc->pools);
        new_resrc->properties = zlist_dup (resrc->properties);
        new_resrc->tags = zhash_dup (resrc->tags);
    } else {
        oom ();
    }

    return new_resrc;
}

void resrc_resource_destroy (void *object)
{
    int64_t *id_ptr;
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
        while ((id_ptr = zlist_pop (resrc->jobs)))
            jobid_destroy (id_ptr);
        zlist_destroy (&resrc->jobs);
        while ((id_ptr = zlist_pop (resrc->resrv_jobs)))
            jobid_destroy (id_ptr);
        zlist_destroy (&resrc->resrv_jobs);
        zhash_destroy (&resrc->pools);
        if (resrc->properties)
            zlist_destroy (&resrc->properties);
        zhash_destroy (&resrc->tags);
        free (resrc);
    }
}

static resrc_t resrc_add_resource (zhash_t *resrcs, resrc_t parent,
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
    resrc_t resrc = NULL;
    resrc_tree_t parent_tree = NULL;
    resrc_tree_t resrc_tree = NULL;
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
            pool = resrc_new_pool (items);
            if (pool) {
                zhash_insert (parent->pools, type, pool);
                /* we've added a memory pool and so we're done */
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
    free (fullname);
    free (nodename);
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
    char out[40];
    char *tag;
    int64_t *id_ptr;
    resrc_pool_t *pool;

    if (resrc) {
        uuid_unparse (resrc->uuid, out);
        printf ("resrc type:%s, name:%s, id:%ld, state:%d, uuid: %s",
                resrc->type, resrc->name, resrc->id, resrc->state, out);
        if (zhash_size (resrc->tags)) {
            printf (", tags");
            tag = zhash_first (resrc->tags);
            while (tag) {
                printf (", %s", (char *)zhash_cursor (resrc->tags));
                tag = zhash_next (resrc->tags);
            }
        }
        if (zhash_size (resrc->pools)) {
            printf (", pools");
            pool = zhash_first (resrc->pools);
            do {
                printf (", %s: %ld of %ld", (char *)zhash_cursor (resrc->pools),
                        pool->avail_items, pool->total_items);
            } while ((pool = zhash_next (resrc->pools)));
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
    char *sptype = NULL;        /* sample pool type */
    resrc_pool_t *r_pool;       /* resource pool */
    resrc_pool_t *s_pool;       /* sample pool */

    if (!strcmp (resrc->type, sample->type)) {
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
        if (zhash_size (sample->pools)) {
            if (!zhash_size (resrc->pools)) {
                goto ret;
            }
            /* Be sure the resource has enough available items from
             * all the requested pools.  We re-purpose the sample's
             * avail_items to convey the requested items */
            s_pool = zhash_first (sample->pools);
            do {
                sptype = (char *)zhash_cursor (sample->pools);
                if ((r_pool = zhash_lookup (resrc->pools, sptype))) {
                    if (r_pool->avail_items < s_pool->avail_items)
                        goto ret;
                } else
                    goto ret;
            } while ((s_pool = zhash_next (sample->pools)));
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
    int req_qty = 0;
    resrc_t resrc;
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

    resrc_id_list_destroy ((resource_list_t)resrc_ids);
ret:
    return nfound;
}
#endif

bool resrc_has_pool (resrc_t resrc, char *type)
{
    bool rc = false;

    if (zhash_lookup (resrc->pools, type))
        rc = true;
    return rc;
}

int resrc_select_pool_items (resrc_t resrc, char *type, int64_t items)
{
    resrc_pool_t *pool;
    int rc = -1;
    if ((pool = zhash_lookup (resrc->pools, type))) {
        pool->selctd_items = items;
        rc = 0;
    }
    return rc;
}

/*
 * Attach a job_id to a resource and change its state to allocated.
 * Note that a resource's pool items can be distributed to multiple
 * jos.  We convey the number of items of a pool to allocate in the
 * pool's selctd_items member.  Allocating pool items to a job takes
 * the form of a new job_id/items key value pair being added to the
 * resource pool's allocation table.
 */
int resrc_allocate_resource (resrc_t resrc, int64_t job_id)
{
    int64_t *id_ptr;
    int rc = -1;

    if (resrc && job_id) {
        id_ptr = xzmalloc (sizeof (int64_t));
        *id_ptr = job_id;
        zlist_append (resrc->jobs, id_ptr);
        resrc->state = RESOURCE_ALLOCATED;
        if (zhash_size (resrc->pools)) {
            resrc_pool_t *pool = zhash_first (resrc->pools);
            do {
                if (pool->selctd_items) {
                    int64_t *p_si;

                    if (pool->selctd_items > pool->avail_items)
                        goto ret;
                    p_si = xzmalloc (sizeof (int64_t));
                    *p_si = pool->selctd_items;
                    zhash_insert (pool->allocs, id_ptr, p_si);
                    pool->avail_items -= pool->selctd_items;
                    pool->selctd_items = 0;
                }
            } while ((pool = zhash_next (resrc->pools)));
        }
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
    int64_t *id_ptr;
    int rc = -1;

    if (resrc && job_id) {
        id_ptr = xzmalloc (sizeof (int64_t));
        *id_ptr = job_id;
        zlist_append (resrc->resrv_jobs, id_ptr);
        if (resrc->state != RESOURCE_ALLOCATED)
            resrc->state = RESOURCE_RESERVED;
        if (zhash_size (resrc->pools)) {
            resrc_pool_t *pool = zhash_first (resrc->pools);
            do {
                if (pool->selctd_items) {
                    int64_t *p_si;

                    if (pool->selctd_items > pool->avail_items)
                        goto ret;
                    p_si = xzmalloc (sizeof (int64_t));
                    *p_si = pool->selctd_items;
                    zhash_insert (pool->reservtns, id_ptr, p_si);
                    pool->avail_items -= pool->selctd_items;
                    pool->selctd_items = 0;
                }
            } while ((pool = zhash_next (resrc->pools)));
        }
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

            if (zhash_size (resrc->pools)) {
                int64_t *item;
                resrc_pool_t *pool = zhash_first (resrc->pools);
                do {
                    if ((item = zhash_lookup (pool->allocs, (void *)&rel_job))) {
                        pool->avail_items += *item;
                        zhash_delete (pool->allocs, (void *)&rel_job);
                    }
                } while ((pool = zhash_next (resrc->pools)));
            }
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
    const char *type = NULL;
    JSON jtago = NULL;  /* json tag object */
    json_object_iter iter;
    resrc_t resrc = NULL;

    if (!Jget_str (o, "type", &type))
        goto ret;

    if (parent) {
        if (!strncmp (type, "memory", 6)) {
            int64_t items;
            resrc_pool_t *pool;

            if (Jget_int64 (o, "req_qty", &items)) {
                pool = resrc_new_pool (items);
                if (pool) {
                    zhash_insert (parent->pools, type, pool);
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

