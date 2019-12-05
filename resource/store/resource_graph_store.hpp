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

#ifndef RESOURCE_GRAPH_STORE_HPP
#define RESOURCE_GRAPH_STORE_HPP

#include <string>
#include <memory>
#include "resource/schema/resource_graph.hpp"

namespace Flux {
namespace resource_model {

class resource_reader_base_t;

/*! Resource graph data metadata.
 *  Adjacency_list graph, roots of this graph and various indexing.
 */
struct resource_graph_metadata_t {
    std::map<subsystem_t, vtx_t> roots;
    std::map<subsystem_t, relation_infra_t> v_rt_edges;
    std::map<std::string, std::vector <vtx_t>> by_type;
    std::map<std::string, std::vector <vtx_t>> by_name;
    std::map<std::string, vtx_t> by_path;
};

/*! Resource graph data store.
 *  Adjacency_list graph, roots of this graph and various indexing.
 */
struct resource_graph_db_t {
    resource_graph_t resource_graph;
    resource_graph_metadata_t metadata;

    /*! Return true if s is known subsystem
     */
    bool known_subsystem (const std::string &s);

    /*! Load str into the resource graph
     *
     * \param str    string containing a GRUG specification
     * \param reader resource reader base class object
     * \param rank   assign this rank to all the newly created resource vertices
     * \return       0 on success; non-zero integer on an error
     *                   ENOMEM: out of memory
     *                   EINVAL: invalid input or operation (e.g. 
     *                               hwloc version or json string load error)
     *                   EPROTO: str violates the schema
     */
    int load (const std::string &str,
              std::shared_ptr<resource_reader_base_t> &reader,
              int rank = -1);

    /*! Load str into the resource graph and graft the top-level
     *  vertices to vtx_at.
     * \param str    string containing a GRUG specification
     * \param reader resource reader base class object
     * \param vtx_at parent vtx at which to graft the deserialized graph
     * \param rank   assign this rank to all the newly created resource vertices
     * \return       0 on success; non-zero integer on an error
     *                   ENOMEM: out of memory
     *                   EINVAL: invalid input or operation (e.g. 
     *                               hwloc version or json string load error)
     *                   EPROTO: str violates the schema
     */
    int load (const std::string &str,
              std::shared_ptr<resource_reader_base_t> &reader,
              vtx_t &vtx_at, int rank = -1);
};

}
}

#endif // RESOURCE_GRAPH_STORE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
