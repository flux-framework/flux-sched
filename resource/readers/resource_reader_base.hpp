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

#ifndef RESOURCE_READER_BASE_HPP
#define RESOURCE_READER_BASE_HPP

#include <set>
#include <string>
#include <cerrno>
#include "resource/schema/resource_graph.hpp"
#include "resource/store/resource_graph_store.hpp"

namespace Flux {
namespace resource_model {


/*!  Base resource reader class.
 */
class resource_reader_base_t {
public:
    /*! Unpack str into a resource graph.
     *
     * \param g      resource graph
     * \param m      resource graph meta data
     * \param str    resource set string
     * \param rank   assign rank to all of the newly created resource vertices
     * \return       0 on success; non-zero integer on an error
     */
    virtual int unpack (resource_graph_t &g, resource_graph_metadata_t &m,
                        const std::string &str, int rank = -1) = 0;

    /*! Unpack str into a resource graph and graft
     *  the top-level vertices to vtx.
     *
     * \param g      resource graph
     * \param m      resource graph meta data
     * \param vtx    parent vtx at which to graft the deserialized graph
     * \param str    resource set string
     * \param rank   assign rank to all of the newly created resource vertices
     * \return       0 on success; non-zero integer on an error
     */
    virtual int unpack_at (resource_graph_t &g, resource_graph_metadata_t &m,
                           vtx_t &vtx, const std::string &str, int rank = -1) = 0;

    /*! Set the whitelist: only resources that are part of this whitelist
     *  will be unpacked into the graph.
     *
     * \param csl    comma separated whitelist string
     * \return       0 on success; non-zero integer on an error
     */
    int set_whitelist (const std::string &csl);

    /*! Is the selected reader format support whitelist
     *
     * \return       true when supported
     */
    virtual bool is_whitelist_supported () = 0;

    /*! Return the error message string.
     */
    const std::string &err_message () const;

    /*! Clear the error message string.
     */
    void clear_err_message ();

protected:
    bool in_whitelist (const std::string &resource);
    std::set<std::string> whitelist;
    std::string m_err_msg = "";
};

} // namesapce resource_model
} // namespace Flux


#endif // RESOURCE_READER_BASE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
