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
 * Parse RFC 31 Constraint from Jobspec into a Constraint object
 */

#ifndef JOBSPEC_CONSTRAINT_HPP
#define JOBSPEC_CONSTRAINT_HPP

#include <iostream>
#include <string>
#include <yaml-cpp/yaml.h>

#include "resource/schema/resource_base.hpp"
#include "parse_error.hpp"

namespace Flux {
namespace Jobspec {

class Constraint {
   public:
    Constraint () = default;
    virtual ~Constraint () = default;
    virtual bool match (const Flux::resource_model::resource_t &resource) const;
    virtual YAML::Node as_yaml () const;
};

std::unique_ptr<Constraint> constraint_parser (const YAML::Node &constraint);

}  // namespace Jobspec
}  // namespace Flux

#endif  // JOBSPEC_CONSTRAINT_HPP
