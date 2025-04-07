/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
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

#include "resource/store/resource_graph_store.hpp"
#include "resource/readers/resource_reader_base.hpp"

using namespace Flux;
using namespace Flux::resource_model;

void resource_graph_metadata_t::set_graph_duration (graph_duration_t &g_duration)
{
    if ((g_duration.graph_start == std::chrono::system_clock::from_time_t (0))
        && (g_duration.graph_end == std::chrono::system_clock::from_time_t (0))) {
        graph_duration.graph_start = std::chrono::system_clock::now ();
        graph_duration.graph_end =
            graph_duration.graph_start + std::chrono::seconds (detail::SYSTEM_MAX_DURATION);
    } else {
        graph_duration.graph_start = g_duration.graph_start;
        graph_duration.graph_end = g_duration.graph_end;
    }
}

void resource_graph_metadata_t::update_node_stats (int count, resource_pool_t::status_t status)
{
    if (status == resource_pool_t::status_t::UP) {
        nodes_up += count;
    } else {
        nodes_up -= count;
    }
}

void resource_graph_metadata_t::initialize_node_stats (resource_graph_t const &g)
{
    const auto by_type_iter = by_type.find (resource_model::node_rt);
    if (by_type_iter == by_type.end ())
        return;
    auto &all_nodes = by_type_iter->second;
    for (auto const &node_vtx : all_nodes) {
        update_node_stats (1, g[node_vtx].status);
    }
}

bool resource_graph_db_t::known_subsystem (subsystem_t s)
{
    return (metadata.roots.find (s) != metadata.roots.end ()) ? true : false;
}

int resource_graph_db_t::load (const std::string &str,
                               std::shared_ptr<resource_reader_base_t> &reader,
                               int rank)
{
    int rc = reader->unpack (resource_graph, metadata, str, rank);
    metadata.initialize_node_stats (resource_graph);
    return rc;
};

int resource_graph_db_t::load (const std::string &str,
                               std::shared_ptr<resource_reader_base_t> &reader,
                               vtx_t &vtx_at,
                               int rank)
{
    int rc = reader->unpack_at (resource_graph, metadata, vtx_at, str, rank);
    metadata.initialize_node_stats (resource_graph);
    return rc;
};

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
