/*****************************************************************************\
 *  Copyright (c) 2019 Lawrence Livermore National Security, LLC.  Produced at
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

#ifndef RESOURCE_READER_JGF_HPP
#define RESOURCE_READER_JGF_HPP

#include <string>
#include <jansson.h>
#include "resource/schema/resource_graph.hpp"
#include "resource/readers/resource_reader_base.hpp"

struct fetch_helper_t;
struct vmap_val_t;

namespace Flux {
namespace resource_model {


/*! JGF resource reader class.
 */
class resource_reader_jgf_t : public resource_reader_base_t {
public:

    virtual ~resource_reader_jgf_t ();

    /*! Unpack str into a resource graph.
     *
     * \param g      resource graph
     * \param m      resource graph meta data
     * \param str    string containing a JGF specification
     * \param rank   assign rank to all of the newly created resource vertices
     * \return       0 on success; non-zero integer on an error
     *                   ENOMEM: out of memory
     *                   EINVAL: input input or operation
     */
    virtual int unpack (resource_graph_t &g, resource_graph_metadata_t &m,
                        const std::string &str, int rank = -1);

    /*! Unpack str into a resource graph and graft
     *  the top-level vertices to vtx.
     *
     * \param g      resource graph
     * \param m      resource graph meta data
     * \param vtx    parent vtx at which to graft the deserialized graph
     * \param str    string containing a JGF specification
     * \param rank   assign this rank to all the newly created resource vertices
     * \return       -1 with errno=ENOTSUP (Not supported yet)
     */
    virtual int unpack_at (resource_graph_t &g, resource_graph_metadata_t &m,
                           vtx_t &vtx, const std::string &str, int rank = -1);

    /*! Update resource graph g with str.
     *
     * \param g      resource graph
     * \param m      resource graph meta data
     * \param str    resource set string
     * \param jobid  jobid of str
     * \param at     start time of this job
     * \param dur    duration of this job
     * \param rsv    true if this update is for a reservation.
     * \param trav_token
     *               token to be used by traverser
     * \return       0 on success; non-zero integer on an error
     */
    virtual int update (resource_graph_t &g, resource_graph_metadata_t &m,
                        const std::string &str, int64_t jobid, int64_t at,
                        uint64_t dur, bool rsv, uint64_t trav_token);

    /*! Is the selected reader format support allowlist
     *
     * \return       false
     */
    virtual bool is_allowlist_supported ();

private:
    int fetch_jgf (const std::string &str,
                   json_t **jgf_p, json_t **nodes_p, json_t **edges_p);
    int unpack_vtx (json_t *element, fetch_helper_t &f);
    vtx_t create_vtx (resource_graph_t &g, const fetch_helper_t &fetcher);
    bool is_root (const std::string &path);
    int check_root (vtx_t v, resource_graph_t &g,
                    std::map<std::string, bool> &is_roots);
    int add_graph_metadata (vtx_t v, resource_graph_t &g,
                            resource_graph_metadata_t &m);
    int add_vtx (resource_graph_t &g, resource_graph_metadata_t &m,
                 std::map<std::string, vmap_val_t> &vmap,
                 const fetch_helper_t &fetcher);
    int find_vtx (resource_graph_t &g, resource_graph_metadata_t &m,
                  std::map<std::string, vmap_val_t> &vmap,
                  const fetch_helper_t &fetcher, vtx_t &ret_v);
    int update_vtx_plan (vtx_t v, resource_graph_t &g,
                         const fetch_helper_t &fetcher, uint64_t jobid,
                         int64_t at, uint64_t dur, bool rsv);
    int update_vtx (resource_graph_t &g, resource_graph_metadata_t &m,
                    std::map<std::string, vmap_val_t> &vmap,
                    const fetch_helper_t &fetcher, uint64_t jobid, int64_t at,
                    uint64_t dur, bool rsv);
    int unpack_vertices (resource_graph_t &g, resource_graph_metadata_t &m,
                         std::map<std::string, vmap_val_t> &vmap, json_t *nodes);
    int undo_vertices (resource_graph_t &g,
                       std::map<std::string, vmap_val_t> &vmap,
                       uint64_t jobid, bool rsv);
    int update_vertices (resource_graph_t &g, resource_graph_metadata_t &m,
                         std::map<std::string, vmap_val_t> &vmap, json_t *nodes,
                         int64_t jobid, int64_t at, uint64_t dur, bool rsv);
    int update_vertices (resource_graph_t &g, resource_graph_metadata_t &m,
                         std::map<std::string, vmap_val_t> &vmap, json_t *nodes,
                         int64_t jobid, int64_t at, uint64_t dur);
    int unpack_edge (json_t *element, std::map<std::string, vmap_val_t> &vmap,
                     std::string &source, std::string &target, json_t **name);
    int update_src_edge (resource_graph_t &g, resource_graph_metadata_t &m,
                         std::map<std::string, vmap_val_t> &vmap,
                         std::string &source, uint64_t token);
    int update_tgt_edge (resource_graph_t &g, resource_graph_metadata_t &m,
                         std::map<std::string, vmap_val_t> &vmap,
                         std::string &source, std::string &target,
                         uint64_t token);
    int unpack_edges (resource_graph_t &g, resource_graph_metadata_t &m,
                      std::map<std::string, vmap_val_t> &vmap, json_t *edges);
    int update_edges (resource_graph_t &g, resource_graph_metadata_t &m,
                      std::map<std::string, vmap_val_t> &vmap,
                      json_t *edges, uint64_t token);
};

} // namespace resource_model
} // namespace Flux

#endif // RESOURCE_READER_GRUG_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
