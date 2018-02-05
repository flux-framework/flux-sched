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

#ifndef RESOURCE_GEN_HPP
#define RESOURCE_GEN_HPP

#include <string>
#include <boost/graph/depth_first_search.hpp>
#include "resource_graph.hpp"
#include "resource_gen_spec.hpp"

namespace Flux {
namespace resource_model {

class resource_generator_t {
public:
    resource_generator_t ();
    resource_generator_t (const resource_generator_t &o);
    const resource_generator_t &operator=(const resource_generator_t &o);
    ~resource_generator_t ();
    int read_graphml (const std::string &f, resource_graph_db_t &db);
    const std::string &err_message () const;
private:
    resource_gen_spec_t m_gspec;
    std::string m_err_msg = "";
};

} // namespace resource_model
} // namespace Flux

#endif // RESOURCE_GEN_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
