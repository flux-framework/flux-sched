/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
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

#include "resource/schema/resource_data.hpp"

namespace Flux {
namespace resource_model {

////////////////////////////////////////////////////////////////////////////////
// Resource Pool Method Definitions
////////////////////////////////////////////////////////////////////////////////

resource_pool_t::resource_pool_t () = default;
resource_pool_t::resource_pool_t (const resource_pool_t &o) = default;
resource_pool_t::resource_pool_t (resource_pool_t &&o) = default;
resource_pool_t &resource_pool_t::operator= (const resource_pool_t &o) = default;
resource_pool_t &resource_pool_t::operator= (resource_pool_t &&o) = default;
resource_pool_t::~resource_pool_t () = default;

////////////////////////////////////////////////////////////////////////////////
// Resource Relation Method Definitions
////////////////////////////////////////////////////////////////////////////////

resource_relation_t::resource_relation_t () = default;
resource_relation_t::resource_relation_t (const resource_relation_t &o) = default;
resource_relation_t::resource_relation_t (resource_relation_t &&o) = default;
resource_relation_t &resource_relation_t::operator= (const resource_relation_t &o) = default;
resource_relation_t &resource_relation_t::operator= (resource_relation_t &&o) = default;
resource_relation_t::~resource_relation_t () = default;

const resource_pool_t::string_to_status resource_pool_t::str_to_status =
    {{"up", resource_pool_t::status_t::UP},
     {"down", resource_pool_t::status_t::DOWN},
     {"lost", resource_pool_t::status_t::LOST}};

const std::string resource_pool_t::status_to_str (status_t s)
{
    std::string str;
    switch (s) {
        case status_t::UP:
            str = "UP";
            break;
        case status_t::DOWN:
            str = "DOWN";
            break;
        case status_t::LOST:
            str = "LOST";
            break;
        default:
            str = "";
            errno = EINVAL;
            break;
    }
    return str;
}

}  // namespace resource_model
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
