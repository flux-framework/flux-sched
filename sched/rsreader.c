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

/*
 * rsreader.c - resource reader modes API implementation
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <libgen.h>
#include <hwloc.h>

#include "src/common/libutil/log.h"
#include "src/common/libutil/shortjson.h"
#include "src/common/libutil/xzmalloc.h"
#include "resrc.h"
#include "resrc_tree.h"
#include "resrc_reqst.h"
#include "rs2rank.h"
#include "rsreader.h"

/******************************************************************************
 *                                                                            *
 *                             Utility functions                              *
 *                                                                            *
 ******************************************************************************/

static inline const char *get_hn (hwloc_topology_t topo)
{
    hwloc_obj_t obj;
    const char *hn = NULL;
    obj = hwloc_get_obj_by_type (topo, HWLOC_OBJ_MACHINE, 0);
    if (obj)
        hn = hwloc_obj_get_info_by_name (obj, "HostName");
    return hn;
}

static inline void create_req4allnodes (json_object *reqobj)
{
    json_object *req1;
    json_object *req2;
    Jadd_str (reqobj, "type", "node");
    Jadd_int (reqobj, "req_qty", 1);
    req1 = Jnew ();
    Jadd_str (req1, "type", "socket");
    Jadd_int (req1, "req_qty", 1);
    req2 = Jnew ();
    Jadd_str (req2, "type", "core");
    Jadd_int (req2, "req_qty", 1);
    json_object_object_add (req1, "req_child", req2);
    json_object_object_add (reqobj, "req_child", req1);
}

static inline void create_req4allsocks (json_object *reqobj)
{
    json_object *req1;
    Jadd_str (reqobj, "type", "socket");
    Jadd_int (reqobj, "req_qty", 1);
    req1 = Jnew ();
    Jadd_str (req1, "type", "core");
    Jadd_int (req1, "req_qty", 1);
    json_object_object_add (reqobj, "req_child", req1);
}

static inline void create_req4allcores (json_object *reqobj)
{
    Jadd_str (reqobj, "type", "core");
    Jadd_int (reqobj, "req_qty", 1);
}

static int find_all_nodes (resrc_t *root, resrc_tree_t **ot)
{
    json_object *reqobj = NULL;
    int64_t size = 0;
    resrc_reqst_t *req = NULL;

    reqobj = Jnew ();
    create_req4allnodes (reqobj);
    req = resrc_reqst_from_json (reqobj, NULL);
    size = resrc_tree_search (root, req, ot, false);
    resrc_reqst_destroy (req);
    Jput (reqobj);

    return (size > 0) ? 0 : -1;
}

static int find_all_sockets_cores (resrc_t *node, int *nsocks, int *ncs)
{
    json_object *reqobj= NULL;
    resrc_reqst_t *req = NULL;
    resrc_tree_t *st = NULL;
    resrc_tree_t *ct = NULL;

    reqobj = Jnew ();
    create_req4allsocks (reqobj);
    req = resrc_reqst_from_json (reqobj, NULL);
    *nsocks = resrc_tree_search (node, req, &st, false);
    resrc_reqst_destroy (req);
    resrc_tree_destroy (st, false);
    Jput (reqobj);

    reqobj = Jnew ();
    create_req4allcores (reqobj);
    req = resrc_reqst_from_json (reqobj, NULL);
    *ncs = resrc_tree_search (node, req, &ct, false);
    resrc_reqst_destroy (req);
    resrc_tree_destroy (ct, false);
    Jput (reqobj);

    return (*nsocks > 0 && *ncs > 0) ? 0 : -1;
}


/******************************************************************************
 *                                                                            *
 *                   Resource Reader Public APIs                              *
 *                                                                            *
 ******************************************************************************/

int rsreader_resrc_bulkload (const char *path, char *uri, char **r_uri,
                             resrc_t **r_resrc)
{
    *r_uri = uri ? uri : xstrdup ("default");
    return (*r_resrc = resrc_generate_rdl_resources (path, *r_uri)) ? 0 : -1;
}

int rsreader_hwloc_bulkload (const char *buf, size_t len, rsreader_t r_mode,
                             char **r_uri, resrc_t **r_resrc, machs_t *machs)
{
    /* NYI */
    return -1;
}

int rsreader_resrc_load (const char *path, char *uri, uint32_t rank, char **r_uri,
                         resrc_t **r_resrc)
{
    /* NYI */
    return -1;
}

int rsreader_hwloc_load (const char *buf, size_t len, uint32_t rank,
                         rsreader_t r_mode, resrc_t **r_resrc, machs_t *machs,
                         char **err_str)
{
    int rc = -1;
    rssig_t *sig = NULL;
    hwloc_topology_t topo;

    if (!machs)
        goto done;

    if (hwloc_topology_init (&topo) != 0)
        goto done;
    if (hwloc_topology_set_xmlbuffer (topo, buf, len) != 0)
        goto err;
    if (hwloc_topology_load (topo) != 0)
        goto err;
    if (rs2rank_set_signature ((char*)buf, len, topo, &sig) != 0)
        goto err;
    if (rs2rank_tab_update (machs, get_hn (topo), sig, rank) != 0)
        goto err;

    if (r_mode == RSREADER_HWLOC) {
        const char *s = rs2rank_get_digest (sig);
        if (!resrc_generate_hwloc_resources (*r_resrc, topo, s, err_str))
            goto err;
    }

    rc = 0;
err:
    hwloc_topology_destroy (topo);
done:
    return rc;
}

static int rsreader_set_granular_digest (machs_t *machs, resrc_tree_t *rt,
                                   char **err_str)
{
    char *e_str = NULL;
    const char *digest = NULL;
    int nsocks = 0, ncs = 0;
    int rc = -1;
    resrc_t *r = NULL;

    if (rt) {
        r = resrc_tree_resrc (rt);
        if (strcmp (resrc_type (r), "node")) {
            if (resrc_tree_num_children (rt)) {
                resrc_tree_list_t *children = resrc_tree_children (rt);
                if (children) {
                    resrc_tree_t *child = resrc_tree_list_first (children);

                    while (child) {
                        rc = rsreader_set_granular_digest (machs, child, err_str);
                        if (rc)
                            break;
                        child = resrc_tree_list_next (children);
                    }
                }
            }
        } else if (!find_all_sockets_cores (r, &nsocks, &ncs)) {
            /* matches based on the hostname, number of sockets and
             * cores.  This linking isn't used by hwloc reader so
             * count-based matching is okay
             */
            digest = rs2rank_tab_eq_by_count(machs, resrc_name (r), nsocks, ncs);
            if (digest) {
                (void) resrc_set_digest (r, xasprintf ("%s", digest));
                rc = 0;
            } else
                e_str = xasprintf ("%s: Can't find a matching resrc for "
                                   "<%s,%d,%d>", __FUNCTION__, resrc_name (r),
                                   nsocks, ncs);
        }
    }

    if (err_str)
        *err_str = e_str;
    else
        free (e_str);

    return rc;
}

int rsreader_link2rank (machs_t *machs, resrc_t *r_resrc, char **err_str)
{
    int rc = -1;
    resrc_tree_t *rt = NULL;

    if (!find_all_nodes (r_resrc, &rt))
        rc = rsreader_set_granular_digest (machs, rt, err_str);
    if (rt)
        resrc_tree_destroy (rt, false);

    return rc;
}

static int rsreader_set_node_digest (machs_t *machs, resrc_tree_t *rt)
{
    const char *digest = NULL;
    int rc = -1;
    resrc_t *r = NULL;

    if (rt) {
        r = resrc_tree_resrc (rt);
        if (strcmp (resrc_type (r), "node")) {
            if (resrc_tree_num_children (rt)) {
                resrc_tree_list_t *children = resrc_tree_children (rt);
                if (children) {
                    resrc_tree_t *child = resrc_tree_list_first (children);

                    while (child) {
                        rc = rsreader_set_node_digest (machs, child);
                        if (rc)
                            break;
                        child = resrc_tree_list_next (children);
                    }
                }
            }
        } else {
            /* Use the digest from the first resource partition of
             * the first node in the machine table
             */
            digest = rs2rank_tab_eq_by_none (machs);
            if (digest) {
                (void) resrc_set_digest (r, xasprintf ("%s", digest));
                rc = 0;
            }
        }
    }

    return rc;
}

int rsreader_force_link2rank (machs_t *machs, resrc_t *r_resrc)
{
    int rc = -1;
    resrc_tree_t *rt = NULL;

    if (!find_all_nodes (r_resrc, &rt))
        rc = rsreader_set_node_digest (machs, rt);
    if (rt)
        resrc_tree_destroy (rt, false);

    return rc;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
