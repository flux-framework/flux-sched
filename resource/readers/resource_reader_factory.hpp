/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef RESOURCE_READER_FACTORY_HPP
#define RESOURCE_READER_FACTORY_HPP

#include <string>
#include <memory>
#include "resource/readers/resource_reader_base.hpp"

namespace Flux {
namespace resource_model {

bool known_resource_reader (const std::string &name);
std::shared_ptr<resource_reader_base_t> create_resource_reader (const std::string &name);

}  // namespace resource_model
}  // namespace Flux

#endif  // RESOURCE_READER_FACTORY_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
