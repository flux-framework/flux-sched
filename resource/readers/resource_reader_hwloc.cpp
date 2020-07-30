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

#include "resource/readers/resource_reader_hwloc.hpp"
#include "resource/store/resource_graph_store.hpp"

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

using namespace Flux;
using namespace resource_model;


/********************************************************************************
 *                                                                              *
 *                       Private HWLOC Reader API                               *
 *                                                                              *
 ********************************************************************************/

int resource_reader_hwloc_t::check_hwloc_version (std::string &m_err_msg)
{
    unsigned int hwloc_version = hwloc_get_api_version ();

    if ((hwloc_version >> 16) != (HWLOC_API_VERSION >> 16)) {
        std::stringstream msg;
        msg << "Compiled for hwloc API 0x"
            << std::hex << HWLOC_API_VERSION
            << " but running on library API 0x"
            << hwloc_version << "; ";
        m_err_msg += msg.str ();
        errno = EINVAL;
        return -1;
    }
    return 0;
}

vtx_t resource_reader_hwloc_t::create_cluster_vertex (
          resource_graph_t &g, resource_graph_metadata_t &m)
{
    // generate cluster root vertex
    const std::string subsys = "containment";
    vtx_t v = add_new_vertex (g, m, boost::
                              graph_traits<resource_graph_t>::
                              null_vertex (),
                              0, subsys, "cluster", "cluster", 1);
    m.roots.emplace (subsys, v);
    m.v_rt_edges.emplace (subsys, relation_infra_t ());

    return v;
}

vtx_t resource_reader_hwloc_t::add_new_vertex (resource_graph_t &g,
                                               resource_graph_metadata_t &m,
                                               const vtx_t &parent, int id,
                                               const std::string &subsys,
                                               const std::string &type,
                                               const std::string &basename,
                                               int size, int rank)
{
    vtx_t v = boost::add_vertex (g);

    // Set properties of the new vertex
    bool is_root = false;
    if (parent == boost::graph_traits<resource_graph_t>::null_vertex ())
        is_root = true;
    std::string istr = (id != -1)? std::to_string (id) : "";
    std::string prefix =  is_root ? "" : g[parent].paths[subsys];

    g[v].type = type;
    g[v].basename = basename;
    g[v].size = size;
    g[v].uniq_id = v;
    g[v].rank = rank;
    g[v].schedule.plans = planner_new (0, INT64_MAX, size, type.c_str ());
    g[v].idata.x_checker = planner_new (0, INT64_MAX,
                                           X_CHECKER_NJOBS, X_CHECKER_JOBS_STR);
    g[v].id = id;
    g[v].name = basename + istr;
    g[v].paths[subsys] = prefix + "/" + g[v].name;
    g[v].idata.member_of[subsys] = "*";
    g[v].status = resource_pool_t::status_t::UP;

    // Indexing for fast look-up
    m.by_path[g[v].paths[subsys]] = v;
    m.by_type[g[v].type].push_back (v);
    m.by_name[g[v].name].push_back (v);
    m.by_rank[rank].push_back (v);
    return v;
}

void resource_reader_hwloc_t::walk_hwloc (resource_graph_t &g,
                                          resource_graph_metadata_t &m,
                                          const hwloc_topology_t topo,
                                          const hwloc_obj_t obj,
                                          const vtx_t parent, int rank)
{
    bool supported_resource = true;
    std::string type, basename;
    int id = obj->logical_index;
    unsigned int size = 1;

    switch(obj->type) {
    case HWLOC_OBJ_MACHINE: {
        // TODO: add signature to support multiple ranks per node
        const char *hwloc_name = hwloc_obj_get_info_by_name (obj, "HostName");
        if (!hwloc_name || !in_allowlist ("node")) {
            supported_resource = false;
            break;
        }
        type = "node";
        basename = hwloc_name;
        id = -1; // TODO: is this the right thing to do?
        break;
    }
    case HWLOC_OBJ_GROUP: {
        if (!in_allowlist ("group")) {
            supported_resource = false;
            break;
        }
        type = "group";
        basename = type;
        break;
    }
    case HWLOC_OBJ_NUMANODE: {
        if (!in_allowlist ("numanode")) {
            supported_resource = false;
            break;
        }
        type = "numanode";
        basename = type;
        break;
    }
    case HWLOC_OBJ_PACKAGE: {
        if (!in_allowlist ("socket")) {
            supported_resource = false;
            break;
        }
        type = "socket";
        basename = type;
        break;
    }
#if HWLOC_API_VERSION < 0x00020000
    case HWLOC_OBJ_CACHE: {
        std::string r = "L" + std::to_string (obj->attr->cache.depth) + "cache";
        if (!in_allowlist (r)) {
            supported_resource = false;
            break;
        }
        type = "cache";
        basename = r;
        size = obj->attr->cache.size / 1024;
        break;
    }
#else
    case HWLOC_OBJ_L1CACHE: {
        if (!in_allowlist ("L1cache")) {
            supported_resource = false;
            break;
        }
        type = "cache";
        basename = "L1" + type;
        size = obj->attr->cache.size / 1024;
        break;
    }
    case HWLOC_OBJ_L2CACHE: {
        if (!in_allowlist ("L2cache")) {
            supported_resource = false;
            break;
        }
        type = "cache";
        basename = "L2" + type;
        size = obj->attr->cache.size / 1024;
        break;
    }
    case HWLOC_OBJ_L3CACHE: {
        if (!in_allowlist ("L3cache")) {
            supported_resource = false;
            break;
        }
        type = "cache";
        basename = "L3" + type;
        size = obj->attr->cache.size / 1024;
        break;
    }
#endif
    case HWLOC_OBJ_CORE: {
        if (!in_allowlist ("core")) {
            supported_resource = false;
            break;
        }
        type = "core";
        basename = type;
        break;
    }
    case HWLOC_OBJ_PU: {
        if (!in_allowlist ("pu")) {
            supported_resource = false;
            break;
        }
        type = "pu";
        basename = type;
        break;
    }
    case HWLOC_OBJ_OS_DEVICE: {
        supported_resource = false;
        if (!obj->attr) {
            break;
        } else if (obj->attr->osdev.type == HWLOC_OBJ_OSDEV_COPROC) {
            if (!in_allowlist ("gpu")) {
                break;
            }
            /* hwloc doesn't provide the logical index only amongst CoProc
             * devices so we parse this info from the name until hwloc provide
             * better support.
             */
            if (strncmp (obj->name, "cuda", 4) == 0)
                id = atoi (obj->name + 4);
            else if (strncmp (obj->name, "opencl", 6) == 0) {
                /* Naming convention of opencl devices for hwloc:
                 * "opencl" followed by a platform ID followed by a device ID.
                 * Then, the letter d delimits platform id and device id.
                 */
                const char *delim = strchr (obj->name + 6, 'd');
                id = atoi (delim + 1);
            }
            type = "gpu";
            basename = type;
            supported_resource = true;
        } else if (obj->attr->osdev.type == HWLOC_OBJ_OSDEV_BLOCK) {
            if (!in_allowlist ("storage")) {
                break;
            }
            // Size from hwloc is in kBs (base 10)
            const char* size_str = hwloc_obj_get_info_by_name (obj, "Size");
            long int num_bytes = strtol (size_str, NULL, 10);
            if (size_str == NULL || num_bytes == 0) {
                break;
            }
            size = num_bytes / 1000000; // kBs -> gBs
            type = "storage";
            basename = type;
            supported_resource = true;
        }
        break;
    }
    default: {
        supported_resource = false;
        break;
    }
    }

    // A valid ancestor vertex to pass to the recursive call
    vtx_t valid_ancestor;
    if (!supported_resource) {
        valid_ancestor = parent;
    } else {
        const std::string subsys = "containment";
        vtx_t v = add_new_vertex (g, m, parent,
                                  id, subsys, type, basename, size, rank);
        valid_ancestor = v;
        std::string relation = "contains";
        std::string rev_relation = "in";
        edg_t e;
        bool inserted; // set to false when we try and insert a parallel edge

        tie (e, inserted) = add_edge (parent, v, g);
        g[e].idata.member_of[subsys] = relation;
        g[e].name[subsys] = relation;

        tie (e, inserted) = add_edge (v, parent, g);
        g[e].idata.member_of[subsys] = rev_relation;
        g[e].name[subsys] = rev_relation;
    }

    hwloc_obj_t curr_child = NULL;
    while ((curr_child = hwloc_get_next_child (topo, obj, curr_child)) != NULL) {
      walk_hwloc (g, m, topo, curr_child, valid_ancestor, rank);
    }
}

int resource_reader_hwloc_t::unpack_internal (resource_graph_t &g,
                                              resource_graph_metadata_t &m,
                                              vtx_t &vtx,
                                              const std::string &str, int rank)
{
    int rc = -1;
    size_t len = str.length ();
    hwloc_topology_t topo;
    hwloc_obj_t hwloc_root;

    if ( hwloc_topology_init (&topo) != 0 ) {
        errno = ENOMEM;
        m_err_msg += "Error initializing hwloc topology; ";
        goto done;
    }
#if HWLOC_API_VERSION < 0x20000
    if ( hwloc_topology_set_flags (topo, HWLOC_TOPOLOGY_FLAG_IO_DEVICES) != 0) {
        errno = EINVAL;
        m_err_msg += "Error setting hwloc topology flag; ";
        goto done;
    }
#else
    if (hwloc_topology_set_io_types_filter (topo,
                                            HWLOC_TYPE_FILTER_KEEP_IMPORTANT)
        < 0) {
        errno = EINVAL;
        m_err_msg += "hwloc_topology_set_io_types_filter; ";
        goto done;
    }
    if (hwloc_topology_set_cache_types_filter (topo,
                                               HWLOC_TYPE_FILTER_KEEP_STRUCTURE)
        < 0) {
        errno = EINVAL;
        m_err_msg += "hwloc_topology_set_cache_types_filter; ";
        goto done;
    }
    if (hwloc_topology_set_icache_types_filter (topo,
                                                HWLOC_TYPE_FILTER_KEEP_STRUCTURE)
        < 0) {
        errno = EINVAL;
        m_err_msg += "hwloc_topology_set_icache_types_filter; ";
        goto done;
    }
#endif
    if ( hwloc_topology_set_xmlbuffer (topo, str.c_str (), len) != 0) {
        errno = EINVAL;
        m_err_msg += "Error setting xmlbuffer; ";
        goto done;
    }
    if ( hwloc_topology_load (topo) != 0) {
        hwloc_topology_destroy (topo);
        m_err_msg += "Error hwloc load: rank " + std::to_string (rank) + "; ";
        goto done;
    }

    hwloc_root = hwloc_get_root_obj (topo);
    walk_hwloc (g, m, topo, hwloc_root, vtx, rank);
    hwloc_topology_destroy (topo);
    rc = 0;

done:
    return rc;
}


/********************************************************************************
 *                                                                              *
 *                         Public HWLOC Reader API                              *
 *                                                                              *
 ********************************************************************************/

resource_reader_hwloc_t::~resource_reader_hwloc_t ()
{

}

int resource_reader_hwloc_t::unpack (resource_graph_t &g,
                                     resource_graph_metadata_t &m,
                                     const std::string &str, int rank)
{
    int rc = 0;
    vtx_t cluster_vertex;

    if (check_hwloc_version (m_err_msg) < 0) {
        return -1;
    }

    cluster_vertex = create_cluster_vertex (g, m);
    if (str != "")
        rc = unpack_internal (g, m, cluster_vertex, str, rank);
    return rc;
}

int resource_reader_hwloc_t::unpack_at (resource_graph_t &g,
                                        resource_graph_metadata_t &m,
                                        vtx_t &vtx,
                                        const std::string &str, int rank)
{
    if (check_hwloc_version (m_err_msg) < 0) {
        return -1;
    }
    return unpack_internal (g, m, vtx, str, rank);
}

int resource_reader_hwloc_t::update (resource_graph_t &g,
                                     resource_graph_metadata_t &m,
                                     const std::string &str, int64_t jobid,
                                     int64_t at, uint64_t dur, bool rsv,
                                     uint64_t token)
{
    errno = ENOTSUP; // GRUG reader currently does not support update
    return -1;
}

bool resource_reader_hwloc_t::is_allowlist_supported ()
{
    return true;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
