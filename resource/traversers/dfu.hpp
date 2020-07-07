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

#ifndef DFU_TRAVERSE_HPP
#define DFU_TRAVERSE_HPP

#include <iostream>
#include <cstdlib>
#include "resource/traversers/dfu_impl.hpp"

namespace Flux {
namespace resource_model {

/*! Depth-First-and-Up traverser. Perform depth-first visit on the dominant
 *  subsystem and upwalk on each and all of the auxiliary subsystems selected
 *  by the matcher callback object (dfu_match_cb_t). Corresponding match
 *  callback methods are invoked at various well-defined graph visit events.
 */
class dfu_traverser_t : protected detail::dfu_impl_t
{
public:
    dfu_traverser_t ();
    dfu_traverser_t (std::shared_ptr<f_resource_graph_t> g,
                     std::shared_ptr<resource_graph_db_t> db,
                     std::shared_ptr<dfu_match_cb_t> m);
    dfu_traverser_t (const dfu_traverser_t &o);
    dfu_traverser_t (dfu_traverser_t &&o) = default;
    dfu_traverser_t &operator= (const dfu_traverser_t &o);
    dfu_traverser_t &operator= (dfu_traverser_t &&o) = default;
    ~dfu_traverser_t ();

    const std::shared_ptr<const f_resource_graph_t> get_graph () const;
    const std::shared_ptr<const resource_graph_db_t> get_graph_db () const;
    const std::shared_ptr<const dfu_match_cb_t> get_match_cb () const;
    const std::string &err_message () const;

    void set_graph (std::shared_ptr<f_resource_graph_t> g);
    void set_graph_db (std::shared_ptr<resource_graph_db_t> db);
    void set_match_cb (std::shared_ptr<dfu_match_cb_t> m);
    void clear_err_message ();

    /*! Prime the resource graph with subtree plans. Assume that resource graph,
     *  roots and match callback have already been registered. The subtree
     *  plans are instantiated on certain resource vertices and updated with the
     *  information on their subtree resources. For example, the subtree plan
     *  of a compute node resource vertex can be configured to track the number
     *  of available compute cores in aggregate at its subtree. dfu_match_cb_t
     *  provides an interface to tell this initializer what subtree resources
     *  to track at higher-level resource vertices.
     *
     *  \return          0 on success; -1 on error.
     *                       EINVAL: graph, roots or match callback not set.
     *                       ENOTSUP: roots does not contain a subsystem
     *                                the match callback object need to use.
     */
    int initialize ();

    /*! Prime the resource graph with subtree plans. Assume that resource graph,
     *  roots and match callback have already been registered. The subtree
     *  plans are instantiated on certain resource vertices and updated with the
     *  information on their subtree resources. For example, the subtree plan
     *  of a compute node resource vertex can be configured to track the number
     *  of available compute cores in aggregate at its subtree. dfu_match_cb_t
     *  provides an interface to tell this initializer what subtree resources
     *  to track at higher-level resource vertices.
     *
     *  \param g         resource graph of f_resource_graph_t type.
     *  \param roots     map of root vertices, each is a root of a subsystem.
     *  \param m         match callback object of dfu_match_cb_t type.
     *  \return          0 on success; -1 on error.
     *                       EINVAL: graph, roots or match callback not set.
     *                       ENOTSUP: roots does not contain a subsystem
     *                                the match callback uses.
     */
    int initialize (std::shared_ptr<f_resource_graph_t> g,
                    std::shared_ptr<resource_graph_db_t> db,
                    std::shared_ptr<dfu_match_cb_t> m);

    /*! Begin a graph traversal for the jobspec and either allocate or
     *  reserve the resources in the resource graph. Best-matching resources
     *  are selected in accordance with the scoring done by the match callback
     *  methods. Initialization must have successfully finished before this
     *  method is called.
     *
     *  \param jobspec   Jobspec object.
     *  \param writers   vertex/edge writers to emit the matched labels
     *  \param op        schedule operation:
     *                       allocate or allocate_orelse_reserve.
     *  \param id        job ID to use for the schedule operation.
     *  \param at[out]   when the job is scheduled if reserved.
     *  \return          0 on success; -1 on error.
     *                       EINVAL: graph, roots or match callback not set.
     *                       ENOTSUP: roots does not contain a subsystem the
     *                                match callback uses.
     *                       EBUSY: cannot match because resources/devices
     *                              are currently in use.
     *                       ENODEV: unsatifiable jobspec becuase no
     *                               resources/devices can satisfy the request.
     */
    int run (Jobspec::Jobspec &jobspec,
             std::shared_ptr<match_writers_t> &writers,
             match_op_t op, int64_t id, int64_t *at);

    /*! Read str which is a serialized allocation data (e.g., written in JGF)
     *  with rd, and traverse the resource graph to update it with this data.
     *
     *  \param str       allocation string such as written in JGF.
     *  \param writers   vertex/edge writers to emit the matched labels
     *  \param reader    reader object that deserialize str to update the graph
     *  \param id        job ID to use for the schedule operation.
     *  \param at        the starting time of this job.
     *  \param duration  the duration of this job
     *  \return          0 on success; -1 on error.
     *                   TODO: Fill in errnos
     */
    int run (const std::string &str,
             std::shared_ptr<match_writers_t> &writers,
             std::shared_ptr<resource_reader_base_t> &reader,
             int64_t id, int64_t at, uint64_t duration);

    /*! Remove the allocation/reservation referred to by jobid and update
     *  the resource state.
     *
     *  \param jobid     job id.
     *  \return          0 on success; -1 on error.
     *                       EINVAL: graph, roots or match callback not set.
     */
    int remove (int64_t jobid);

    /*! Mark the resource status up|down|etc starting at subtree_root.
     *
     *  \param root_path     path to the root of the subtree to update.
     *  \param status        new status value
     *  \return              0 on success; -1 on error.
     *                       EINVAL: roots or by_path not found.
     */
    int mark (const std::string &root_path, resource_pool_t::status_t status);

    /*! Mark the resource status up|down|etc for subgraph represented by ranks.
     *
     *  \param ranks         set of ranks representing the subgraph to update.
     *  \param status        new status value
     *  \return              0 on success; -1 on error.
     *                       EINVAL: roots or by_path not found.
     */
    int mark (std::set<int64_t> &ranks, resource_pool_t::status_t status);

private:
    int schedule (Jobspec::Jobspec &jobspec, detail::jobmeta_t &meta,
                  bool x, match_op_t op, vtx_t root,
                  std::unordered_map<std::string, int64_t> &dfv);
};

} // namespace resource_model
} // namespace Flux

#endif // DFU_TRAVERSE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
