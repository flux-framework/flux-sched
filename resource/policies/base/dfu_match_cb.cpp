/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

#include "policies/base/dfu_match_cb.hpp"

namespace Flux {
namespace resource_model {

dfu_match_cb_t::dfu_match_cb_t () : m_trav_level (0)
{
}

dfu_match_cb_t::dfu_match_cb_t (const std::string &name) : matcher_data_t (name), m_trav_level (0)
{
}

dfu_match_cb_t::dfu_match_cb_t (const dfu_match_cb_t &o) : matcher_data_t (o)
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

int dfu_match_cb_t::dom_finish_graph (subsystem_t subsystem,
                                      const std::vector<Flux::Jobspec::Resource> &resources,
                                      const resource_graph_t &g,
                                      scoring_api_t &dfu)
{
    return 0;
}

int dfu_match_cb_t::dom_finish_slot (subsystem_t subsystem, scoring_api_t &dfu)
{
    return 0;
}

int dfu_match_cb_t::dom_discover_vtx (vtx_t u,
                                      subsystem_t subsystem,
                                      const std::vector<Flux::Jobspec::Resource> &resources,
                                      const resource_graph_t &g)
{
    m_trav_level++;
    return 0;
}

int dfu_match_cb_t::dom_finish_vtx (vtx_t u,
                                    subsystem_t subsystem,
                                    const std::vector<Flux::Jobspec::Resource> &resources,
                                    const resource_graph_t &g,
                                    scoring_api_t &dfu)
{
    m_trav_level--;
    return 0;
}

int dfu_match_cb_t::aux_discover_vtx (vtx_t u,
                                      subsystem_t subsystem,
                                      const std::vector<Flux::Jobspec::Resource> &resources,
                                      const resource_graph_t &g)

{
    m_trav_level++;
    return 0;
}

int dfu_match_cb_t::aux_finish_vtx (vtx_t u,
                                    subsystem_t subsystem,
                                    const std::vector<Flux::Jobspec::Resource> &resources,
                                    const resource_graph_t &g,
                                    scoring_api_t &dfu)
{
    m_trav_level--;
    return 0;
}

int dfu_match_cb_t::set_stop_on_k_matches (unsigned int k)
{
    // Unless the derived class supports limited traversal,
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

}  // namespace resource_model
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
