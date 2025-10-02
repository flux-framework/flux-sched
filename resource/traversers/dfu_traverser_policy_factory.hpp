/*****************************************************************************\
 * Copyright 2025 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef DFU_TRAVERSER_POLICY_FACTORY_HPP
#define DFU_TRAVERSER_POLICY_FACTORY_HPP

#include <string>
#include <memory>
#include "resource/traversers/dfu_impl.hpp"
#include "resource/traversers/dfu_flexible.hpp"

namespace Flux {
namespace resource_model {
namespace detail {

const std::string SIMPLE = "simple";
const std::string FLEXIBLE = "flexible";

bool known_traverser_policy (const std::string &policy);

/*! Factory method for creating a traverser object
 *  based on the specified policy.
 *  This overload creates a traverser with the default constructor.
 *
 *  \param policy The traverser policy to use (e.g., FLEXIBLE, SIMPLE).
 *  \return A shared pointer to the created traverser, or nullptr on failure.
 */
std::shared_ptr<dfu_impl_t> create_traverser (const std::string &policy = "simple");

/*! Factory method for creating a traverser object
 *  based on the specified policy, resource graph database,
 *  and match callback.
 *  This overload creates a traverser with a specified resource
 *  graph db and match callback.
 *
 *  \param db Shared pointer to the resource graph database.
 *  \param m Shared pointer to the match callback.
 *  \param policy The traverser policy to use (e.g., FLEXIBLE, SIMPLE).
 *  \return A shared pointer to the created traverser, or nullptr on failure.
 */
std::shared_ptr<dfu_impl_t> create_traverser (std::shared_ptr<resource_graph_db_t> db,
                                              std::shared_ptr<dfu_match_cb_t> m,
                                              const std::string &policy = "simple");

}  // namespace detail
}  // namespace resource_model
}  // namespace Flux

#endif  // DFU_TRAVERSER_POLICY_FACTORY_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
