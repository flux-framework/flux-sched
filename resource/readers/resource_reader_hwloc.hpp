/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef RESOURCE_READER_HWLOC_HPP
#define RESOURCE_READER_HWLOC_HPP

extern "C" {
#include <hwloc.h>
}
#include "resource/readers/resource_reader_base.hpp"

namespace Flux {
namespace resource_model {

/*! Hwloc resource reader class.
 */
class resource_reader_hwloc_t : public resource_reader_base_t {
   public:
    virtual ~resource_reader_hwloc_t ();

    /*! Unpack str into a resource graph.
     *
     * \param g      resource graph
     * \param m      resource graph meta data
     * \param str    string containing hwloc xml
     * \param rank   assign rank to all of the newly created resource vertices
     * \return       0 on success; non-zero integer on an error
     *                   ENOMEM: out of memory
     *                   EINVAL: invalid input or operation (e.g. malformed str,
     *                               hwloc version or operation error)
     */
    virtual int unpack (resource_graph_t &g,
                        resource_graph_metadata_t &m,
                        const std::string &str,
                        int rank = -1);

    /*! Unpack str into a resource graph and graft
     *  the top-level vertices to vtx.
     *
     * \param g      resource graph
     * \param m      resource graph meta data
     * \param vtx    parent vtx at which to graft the deserialized graph
     * \param str    string containing hwloc xml
     * \param rank   assign this rank to all the newly created resource vertices
     * \return       0 on success; non-zero integer on an error
     *                   ENOMEM: out of memory
     *                   EINVAL: invalid input or operation (e.g. malformed str,
     *                               hwloc version or operation error)
     */
    virtual int unpack_at (resource_graph_t &g,
                           resource_graph_metadata_t &m,
                           vtx_t &vtx,
                           const std::string &str,
                           int rank = -1);

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
    virtual int update (resource_graph_t &g,
                        resource_graph_metadata_t &m,
                        const std::string &str,
                        int64_t jobid,
                        int64_t at,
                        uint64_t dur,
                        bool rsv,
                        uint64_t trav_token);

    /*! Partial cancellation of jobid based on R.
     *
     * \param g      resource graph
     * \param m      resource graph meta data
     * \param mod_data struct containing resource types to counts, mod type,
     *                 and set of ranks removed
     * \param R    resource set string
     * \param jobid  jobid of str
     * \return       0 on success; non-zero integer on an error
     */
    virtual int partial_cancel (resource_graph_t &g,
                                resource_graph_metadata_t &m,
                                modify_data_t &mod_data,
                                const std::string &R,
                                int64_t jobid);

    /*! Is the hwloc reader format support allowlist
     *
     * \return       true
     */
    virtual bool is_allowlist_supported ();

   private:
    int check_hwloc_version (std::string &m_err_msg);
    vtx_t create_cluster_vertex (resource_graph_t &g, resource_graph_metadata_t &m);
    vtx_t add_new_vertex (resource_graph_t &g,
                          resource_graph_metadata_t &m,
                          const vtx_t &parent,
                          int64_t id,
                          subsystem_t subsys,
                          resource_type_t type,
                          const std::string &basename,
                          const std::string &name,
                          const std::map<std::string, std::string> &properties,
                          int size,
                          int rank = -1);
    int add_metadata (resource_graph_metadata_t &m,
                      edg_t e,
                      vtx_t src,
                      vtx_t tgt,
                      resource_graph_t &g);
    int walk_hwloc (resource_graph_t &g,
                    resource_graph_metadata_t &m,
                    const hwloc_topology_t topo,
                    const hwloc_obj_t obj,
                    const vtx_t parent,
                    int rank);
    int unpack_internal (resource_graph_t &g,
                         resource_graph_metadata_t &m,
                         vtx_t &vtx,
                         const std::string &str,
                         int rank = -1);
};

}  // namespace resource_model
}  // namespace Flux

#endif  // RESOURCE_READER_HWLOC_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
