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

#include "policies/base/dfu_match_cb.hpp"

namespace Flux {
namespace resource_model {

dfu_match_cb_t::dfu_match_cb_t () : m_trav_level (0)
{

}

dfu_match_cb_t::dfu_match_cb_t (const std::string &name)
    : matcher_data_t (name), m_trav_level (0)
{

}

dfu_match_cb_t::dfu_match_cb_t (const dfu_match_cb_t &o)
    : matcher_data_t (o)
{
    m_trav_level = o.m_trav_level;
}

dfu_match_cb_t &dfu_match_cb_t::operator= (const dfu_match_cb_t &o)
{
    matcher_data_t::operator= (o);
    m_trav_level = o.m_trav_level;
    return *this;
}

dfu_match_cb_t::~dfu_match_cb_t ()
{

}

int dfu_match_cb_t::dom_finish_graph (
    const subsystem_t &subsystem,
    const std::vector<Flux::Jobspec::Resource> &resources,
    const f_resource_graph_t &g,
    scoring_api_t &dfu)
{
    return 0;
}

int dfu_match_cb_t::dom_finish_slot (
    const subsystem_t &subsystem,
    scoring_api_t &dfu)
{
    return 0;
}

int dfu_match_cb_t::dom_discover_vtx (
    vtx_t u,
    const subsystem_t &subsystem,
    const std::vector<Flux::Jobspec::Resource> &resources,
    const f_resource_graph_t &g)
{
    m_trav_level++;
    return 0;
}

int dfu_match_cb_t::dom_finish_vtx (
    vtx_t u,
    const subsystem_t &subsystem,
    const std::vector<Flux::Jobspec::Resource> &resources,
    const f_resource_graph_t &g,
    scoring_api_t &dfu)
{
    m_trav_level--;
    return 0;
}

int dfu_match_cb_t::aux_discover_vtx (
    vtx_t u,
    const subsystem_t &subsystem,
    const std::vector<Flux::Jobspec::Resource> &resources,
    const f_resource_graph_t &g)

{
    m_trav_level++;
    return 0;
}

int dfu_match_cb_t::aux_finish_vtx (
    vtx_t u,
    const subsystem_t &subsystem,
    const std::vector<Flux::Jobspec::Resource> &resources,
    const f_resource_graph_t &g,
    scoring_api_t &dfu)
{
    m_trav_level--;
    return 0;
}

int dfu_match_cb_t::set_stop_on_k_matches (unsigned int k)
{
    // Unless the dervied class supports limited traversal,
    // this knob cannot be set
    return -1;
}

int dfu_match_cb_t::get_stop_on_k_matches () const
{
    return 0;
}

void dfu_match_cb_t::incr ()
{
    m_trav_level++;
}

void dfu_match_cb_t::decr ()
{
    m_trav_level--;
}

std::string dfu_match_cb_t::level ()
{
    int i;
    std::string prefix = "";
    for (i = 0; i < m_trav_level; ++i)
        prefix += "----";
    return prefix;
}

} // resource_model
} // Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
