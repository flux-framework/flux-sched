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
#include <hwloc.h>

#include "src/common/libutil/jsonutil.h"
#include "src/common/libutil/log.h"
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

static inline const char *hwloc_get_hn (hwloc_topology_t topo)
{
    hwloc_obj_t obj;
    const char *hn = NULL;;
    obj = hwloc_get_obj_by_type (topo, HWLOC_OBJ_MACHINE, 0);
    if (obj)
        hn = hwloc_obj_get_info_by_name (obj, "HostName");
    return hn;
}

static inline void create_req4allnodes (JSON reqobj)
{
    JSON req1, req2;
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

static inline void create_req4allsocks (JSON reqobj)
{
    JSON req1;
    Jadd_str (reqobj, "type", "socket");
    Jadd_int (reqobj, "req_qty", 1);
    req1 = Jnew ();
    Jadd_str (req1, "type", "core");
    Jadd_int (req1, "req_qty", 1);
    json_object_object_add (reqobj, "req_child", req1);
}

static inline void create_req4allcores (JSON reqobj)
{
    Jadd_str (reqobj, "type", "core");
    Jadd_int (reqobj, "req_qty", 1);
}

static int find_all_nodes (resrc_tree_t *root,
                           resrc_tree_list_t **ot, size_t *size)
{
    JSON reqobj = NULL;
    resrc_reqst_t *req = NULL;

    reqobj = Jnew ();
    create_req4allnodes (reqobj);
    req = resrc_reqst_from_json (reqobj, NULL);
    *ot = resrc_tree_list_new ();
    *size = resrc_tree_search (resrc_tree_children (root), req, *ot, false);
    resrc_reqst_destroy (req);
    Jput (reqobj);

    return (*size > 0) ? 0 : -1;
}

static int find_all_sockets_cores (resrc_tree_t *node, int *nsocks, int *ncs)
{
    JSON reqobj= NULL;
    resrc_reqst_t *req = NULL;
    resrc_tree_list_t *st = NULL;
    resrc_tree_list_t *ct = NULL;

    reqobj = Jnew ();
    create_req4allsocks (reqobj);
    req = resrc_reqst_from_json (reqobj, NULL);
    st = resrc_tree_list_new ();
    *nsocks = resrc_tree_search (resrc_tree_children (node), req, st, false);
    resrc_reqst_destroy (req);
    Jput (reqobj);

    reqobj = Jnew ();
    create_req4allcores (reqobj);
    req = resrc_reqst_from_json (reqobj, NULL);
    ct = resrc_tree_list_new ();
    *ncs = resrc_tree_search (resrc_tree_children (node), req, ct, false);
    resrc_reqst_destroy (req);
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
                         rsreader_t r_mode, resrc_t **r_resrc, machs_t *machs)
{
    int rc = -1;
    rssig_t *sig = NULL;
    hwloc_topology_t topo;

    if (hwloc_topology_init (&topo) != 0)
        goto done;
    else if (hwloc_topology_set_xmlbuffer (topo, buf, len) != 0)
        goto done;
    else if (hwloc_topology_load (topo) != 0)
        goto done;

    if (!machs)
        goto done;
    else if (rs2rank_set_signiture ((char*)buf, len, topo, &sig) != 0)
        goto done;
    else if (rs2rank_tab_update (machs, hwloc_get_hn (topo), sig, rank) != 0)
        goto done;

    /* TODO: add a new resrc function that works w/ hwloc
     * resrc_generate_hwloc_resources (*r_resrc, topology...
     */
    if (r_mode == RSREADER_HWLOC) {
        if (!resrc_generate_xml_resources (*r_resrc, buf, len))
            goto done;
    }

    hwloc_topology_destroy (topo);
    rc = 0;
done:
    return rc;
}

int rsreader_link2rank (machs_t *machs, resrc_t *r_resrc)
{
    int rc = -1;
    char *hn = NULL;
    size_t ncnt = 0;
    resrc_tree_t *o = NULL;
    int nsocks = 0, ncs = 0;
    const char *digest = NULL;
    resrc_tree_list_t *nl = NULL;

    if (find_all_nodes (resrc_phys_tree (r_resrc), &nl, &ncnt) != 0)
        goto done;
    for (o = resrc_tree_list_first (nl); o; o = resrc_tree_list_next (nl)) {
        if (find_all_sockets_cores (o, &nsocks, &ncs) != 0)
            goto done;
        hn = xasprintf ("%s%"PRId64"", resrc_name (resrc_tree_resrc (o)),
                                       resrc_id (resrc_tree_resrc (o)));
        /* matches based on the hostname, number of sockets and cores */
        digest = rs2rank_tab_eq_by_sign (machs, hn, nsocks, ncs);
        free (hn);
        if (!digest)
            goto done;
        resrc_set_digest (resrc_tree_resrc (o), xasprintf ("%s", digest));
    }
    rc = 0;
done:
    return rc;
}

int rsreader_force_link2rank (machs_t *machs, resrc_t *r_resrc)
{
    int rc = -1;
    size_t ncnt = 0;
    resrc_tree_t *o = NULL;
    const char *digest = NULL;
    resrc_tree_list_t *nl = NULL;

    if (find_all_nodes (resrc_phys_tree (r_resrc), &nl, &ncnt) != 0)
        goto done;
    for (o = resrc_tree_list_first (nl); o; o = resrc_tree_list_next (nl)) {
        if (!(digest = rs2rank_tab_eq_by_none (machs)))
            goto done;
        resrc_set_digest (resrc_tree_resrc (o), xasprintf ("%s", digest));
    }
    rc = 0;
done:
    return rc;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
