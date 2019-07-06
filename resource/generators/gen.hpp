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

#ifndef GEN_HPP
#define GEN_HPP

extern "C" {
#include <hwloc.h>
}
#include <string>
#include <boost/graph/depth_first_search.hpp>
#include "resource/schema/resource_graph.hpp"
#include "resource/generators/spec.hpp"

namespace Flux {
namespace resource_model {

class resource_generator_t {
public:
    resource_generator_t ();
    resource_generator_t (const resource_generator_t &o);
    const resource_generator_t &operator=(const resource_generator_t &o);
    ~resource_generator_t ();
    int read_graphml (const std::string &fn, resource_graph_db_t &db);
    int read_hwloc_xml_file (const char *fn, resource_graph_db_t &db);
    int read_ranked_hwloc_xml (const char *hwloc_xml, int rank,
                               const ggv_t &root_vertex,
                               resource_graph_db_t &db);
    int read_ranked_hwloc_xmls (char **hwloc_strs, int size, resource_graph_db_t &db);
    int set_hwloc_whitelist (const std::string &csl);
    ggv_t create_cluster_vertex(resource_graph_db_t &db);
    const std::string &err_message () const;

private:
    int check_hwloc_version (std::string &m_err_msg);
    ggv_t add_new_vertex(resource_graph_db_t &db, const ggv_t &parent, int id,
                         const std::string &subsys, const std::string &type,
                         const std::string &basename, int size, int rank=-1);
    bool in_whitelist (const std::string &resource);
    void walk_hwloc (const hwloc_obj_t obj,
                     const ggv_t parent, int rank, resource_graph_db_t &db);

    std::set<std::string> hwloc_whitelist;
    resource_gen_spec_t m_gspec;
    // TODO: convert to string stream
    std::string m_err_msg = "";
};


} // namespace resource_model
} // namespace Flux

#endif // GEN_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
