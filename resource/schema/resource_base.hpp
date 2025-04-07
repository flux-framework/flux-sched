/*****************************************************************************\
 * Copyright 2023 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef RESOURCE_BASE_HPP
#define RESOURCE_BASE_HPP

#include <string>
#include <map>

#include "resource/schema/data_std.hpp"

namespace Flux {
namespace resource_model {

//! Base resource type.
//  Allows derived resource types for testing
struct resource_t {
    resource_type_t type;
    std::string basename;
    std::string name;
    std::map<std::string, std::string> properties;
    int64_t id = -1;
    int rank = -1;
};

}  // namespace resource_model
}  // namespace Flux

#endif  // RESOURCE_BASE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
