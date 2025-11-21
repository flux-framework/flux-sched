/*****************************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef RESOURCE_READER_JGF_SHORTHAND_HPP
#define RESOURCE_READER_JGF_SHORTHAND_HPP

#include "resource/readers/resource_reader_jgf.hpp"

namespace Flux {
namespace resource_model {

/*! JGF shorthand resource reader class.
 */
class resource_reader_jgf_shorthand_t : public resource_reader_jgf_t {
   public:
    virtual ~resource_reader_jgf_shorthand_t ();

   protected:
    int fetch_additional_vertices (resource_graph_t &g,
                                   resource_graph_metadata_t &m,
                                   std::map<std::string, vmap_val_t> &vmap,
                                   fetch_helper_t &fetcher,
                                   std::vector<fetch_helper_t> &additional_vertices) override;

    int recursively_collect_vertices (resource_graph_t &g,
                                      vtx_t v,
                                      std::vector<fetch_helper_t> &additional_vertices);
};

}  // namespace resource_model
}  // namespace Flux

#endif  // RESOURCE_READER_JGF_SHORTHAND_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
