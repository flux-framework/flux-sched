/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#include "resource/store/resource_graph_store.hpp"
#include "resource/readers/resource_reader_base.hpp"

using namespace Flux;
using namespace Flux::resource_model;

void resource_graph_metadata_t::set_graph_duration (
                            graph_duration_t &g_duration)
{
    if ( (g_duration.graph_start == std::chrono::system_clock::from_time_t (0))
                                 && (g_duration.graph_end
                              == std::chrono::system_clock::from_time_t (0))) {
        graph_duration.graph_start = std::chrono::system_clock::now ();
        graph_duration.graph_end = graph_duration.graph_start +
                            std::chrono::seconds (detail::SYSTEM_MAX_DURATION);
    } else {
        graph_duration.graph_start = g_duration.graph_start;
        graph_duration.graph_end = g_duration.graph_end;
    }
}

bool resource_graph_db_t::known_subsystem (const std::string &s)
{
    return (metadata.roots.find (s) != metadata.roots.end ())? true : false;
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
