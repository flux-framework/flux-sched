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

enum class R_format { R, R_LITE, R_NATIVE };

const char R_FORMAT[] = "R";
const char R_LITE_FORMAT[] = "R_LITE";
const char R_NATIVE_FORMAT[] = "R_NATIVE";

/*! Depth-First-and-Up traverser. Perform depth-first visit on the dominant
 *  subsystem and upwalk on each and all of the auxiliary subsystems selected
 *  by the matcher callback object (dfu_match_cb_t). Corresponding match
 *  callback methods are invoked at various well-defined graph visit events.
 */
class dfu_traverser_t : protected detail::dfu_impl_t
{
public:
    dfu_traverser_t ();
    dfu_traverser_t (f_resource_graph_t *g,
                     dfu_match_cb_t *m, std::map<subsystem_t, vtx_t> *roots);
    dfu_traverser_t (const dfu_traverser_t &o);
    dfu_traverser_t &operator= (const dfu_traverser_t &o);
    ~dfu_traverser_t ();

    const f_resource_graph_t *get_graph () const;
    const std::map<subsystem_t, vtx_t> *get_roots () const;
    const dfu_match_cb_t *get_match_cb () const;
    const std::string &err_message () const;

    void set_graph (f_resource_graph_t *g);
    void set_roots (std::map<subsystem_t, vtx_t> *roots);
    void set_match_cb (dfu_match_cb_t *m);
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
    int initialize (f_resource_graph_t *g, std::map<subsystem_t, vtx_t> *roots,
                    dfu_match_cb_t *m);

    /*! Begin a graph traversal for the jobspec and either allocate or
     *  reserve the resources in the resource graph. Best-matching resources
     *  are selected in accordance with the scoring done by the match callback
     *  methods. Initialization must have successfully finished before this
     *  method is called.
     *
     *  \param jobspec   Jobspec object.
     *  \param op        schedule operation:
     *                       allocate or allocate_orelse_reserve.
     *  \param id        job ID to use for the schedule operation.
     *  \param at[out]   when the job is scheduled if reserved.
     *  \param ss        stringstream into which emitted R infor is stored.
     *  \return          0 on success; -1 on error.
     *                       EINVAL: graph, roots or match callback not set.
     *                       ENOTSUP: roots does not contain a subsystem the
     *                                match callback uses.
     */
    int run (Jobspec::Jobspec &jobspec, match_op_t op, int64_t id, int64_t *at,
             std::stringstream &ss);

    /*! Remove the allocation/reservation referred to by jobid and update
     *  the resource state.
     *
     *  \param jobid     job id.
     *  \return          0 on success; -1 on error.
     *                       EINVAL: graph, roots or match callback not set.
     */
    int remove (int64_t jobid);

private:
    int schedule (Jobspec::Jobspec &jobspec,
                  detail::jobmeta_t &meta, bool x, match_op_t op,
                  vtx_t root, unsigned int *needs,
                  std::unordered_map<std::string, int64_t> &dfv);
};

bool known_R_format (const std::string &f);

} // namespace resource_model
} // namespace Flux

#endif // DFU_TRAVERSE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
