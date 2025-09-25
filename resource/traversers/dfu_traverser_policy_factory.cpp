/*****************************************************************************\
 * Copyright 2025 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

extern "C" {
#if HAVE_CONFIG_H
#include <config.h>
#endif
}

#include <string>
#include "resource/traversers/dfu_traverser_policy_factory.hpp"

namespace Flux {
namespace resource_model {
namespace detail {

bool known_traverser_policy (const std::string &policy)
{
    bool rc = true;
    if (policy != SIMPLE && policy != FLEXIBLE)
        rc = false;

    return rc;
}

std::shared_ptr<dfu_impl_t> create_traverser (const std::string &policy)
{
    std::shared_ptr<dfu_impl_t> traverser = nullptr;
    try {
        if (policy == FLEXIBLE) {
            traverser = std::make_shared<dfu_flexible_t> ();
        } else if (policy == SIMPLE) {
            traverser = std::make_shared<dfu_impl_t> ();
        }
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        traverser = nullptr;
    }

    return traverser;
}

std::shared_ptr<dfu_impl_t> create_traverser (std::shared_ptr<resource_graph_db_t> db,
                                              std::shared_ptr<dfu_match_cb_t> m,
                                              const std::string &policy)
{
    std::shared_ptr<dfu_impl_t> traverser = nullptr;
    try {
        if (policy == FLEXIBLE) {
            traverser = std::make_shared<dfu_flexible_t> (db, m);
        } else if (policy == SIMPLE) {
            traverser = std::make_shared<dfu_impl_t> (db, m);
        }
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        traverser = nullptr;
    }

    return traverser;
}

}  // namespace detail
}  // namespace resource_model
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
