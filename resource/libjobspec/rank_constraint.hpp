/*****************************************************************************\
 * Copyright 2023 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef RANK_CONSTRAINT_HPP
#define RANK_CONSTRAINT_HPP

#include <flux/idset.h>

#include "jobspec.hpp"
#include "constraint.hpp"

namespace Flux {
namespace Jobspec {

class RankConstraint : public Constraint {
   public:
    RankConstraint (const YAML::Node &);
    RankConstraint () = default;
    ~RankConstraint ()
    {
        idset_destroy (ranks);
    };

   private:
    struct idset *ranks = nullptr;

   public:
    virtual bool match (const Flux::resource_model::resource_t &resource) const;
    virtual YAML::Node as_yaml () const;
};

}  // namespace Jobspec
}  // namespace Flux

#endif  // RANK_CONSTRAINT_HPP
