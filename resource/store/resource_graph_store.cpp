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

#include "resource/store/resource_graph_store.hpp"
#include "resource/readers/resource_reader_base.hpp"

using namespace Flux;
using namespace Flux::resource_model;

resource_graph_metadata_t::resource_graph_metadata_t()
{
    roots = std::make_shared<std::map<subsystem_t, vtx_t>> ();
}

bool resource_graph_db_t::known_subsystem (const std::string &s)
{
    return (metadata.roots->find (s) != metadata.roots->end ())? true : false;
}

int resource_graph_db_t::load (const std::string &str,
                               std::shared_ptr<resource_reader_base_t> &reader,
                               int rank)
{
    return reader->unpack (resource_graph, metadata, str, rank);
};

int resource_graph_db_t::load (const std::string &str,
                               std::shared_ptr<resource_reader_base_t> &reader,
                               vtx_t &vtx_at, int rank)
{
    return reader->unpack_at (resource_graph, metadata, vtx_at, str, rank);
};

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
