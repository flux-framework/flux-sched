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
