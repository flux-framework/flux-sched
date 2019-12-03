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

#ifndef RESOURCE_READER_GRUG_HPP
#define RESOURCE_READER_GRUG_HPP

#include <string>
#include <boost/graph/depth_first_search.hpp>
#include "resource/schema/resource_graph.hpp"
#include "resource/readers/resource_reader_base.hpp"
#include "resource/readers/resource_spec_grug.hpp"

namespace Flux {
namespace resource_model {

/*! GRUG resource reader class.
 */
class resource_reader_grug_t : public resource_reader_base_t {
public:
    virtual ~resource_reader_grug_t ();

    /*! Unpack str into a resource graph.
     *
     * \param g      resource graph
     * \param m      resource graph meta data
     * \param str    string containing a GRUG specification
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
     * \param str    string containing a GRUG specification
     * \param rank   assign this rank to all the newly created resource vertices
     * \return       -1 with errno=ENOTSUP (Not supported yet)
     */
    virtual int unpack_at (resource_graph_t &g, resource_graph_metadata_t &m,
                           vtx_t &vtx, const std::string &str, int rank = -1);

    /*! Is the selected reader format support whitelist
     *
     * \return       false
     */
    virtual bool is_whitelist_supported ();

private:
    resource_gen_spec_t m_gspec;
};

} // namespace resource_model
} // namespace Flux

#endif // RESOURCE_READER_GRUG_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
