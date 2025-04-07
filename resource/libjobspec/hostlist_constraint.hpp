/*****************************************************************************\
 * Copyright 2023 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef HOSTLIST_CONSTRAINT_HPP
#define HOSTLIST_CONSTRAINT_HPP

#include <flux/hostlist.h>

#include "jobspec.hpp"
#include "constraint.hpp"

namespace Flux {
namespace Jobspec {

class HostlistConstraint : public Constraint {
   public:
    HostlistConstraint (const YAML::Node &);
    HostlistConstraint () = default;
    ~HostlistConstraint ()
    {
        hostlist_destroy (hl);
    };

   private:
    struct hostlist *hl = nullptr;

   public:
    virtual bool match (const Flux::resource_model::resource_t &resource) const;
    virtual YAML::Node as_yaml () const;
};

}  // namespace Jobspec
}  // namespace Flux

#endif  // HOSTLIST_CONSTRAINT_HPP
