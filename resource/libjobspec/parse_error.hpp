/*****************************************************************************\
 * Copyright 2023 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

/*
 * separate Flux::Jobspec::parse_error header for circular dependency
 * resolution
 */

#ifndef JOBSPEC_PARSE_ERROR_HPP
#define JOBSPEC_PARSE_ERROR_HPP

#include <stdexcept>

namespace Flux {
namespace Jobspec {

class parse_error : public std::runtime_error {
   public:
    int position;
    int line;
    int column;
    parse_error (const char *msg);
    parse_error (const YAML::Node &node, const char *msg);
};

}  // namespace Jobspec
}  // namespace Flux

#endif  // JOBSPEC_PARSE_ERROR_HPP
