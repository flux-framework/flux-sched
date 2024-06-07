/*****************************************************************************\
 * Copyright 2023 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

#include "rank_constraint.hpp"

using namespace Flux::Jobspec;

static int add_idset_string (struct idset *idset, const char *s)
{
    int rc;
    struct idset *ids;

    if (!(ids = idset_decode (s)))
        return -1;
    rc = idset_add (idset, ids);
    idset_destroy (ids);
    return rc;
}

RankConstraint::RankConstraint (const YAML::Node &values)
{
    if (!(ranks = idset_create (0, IDSET_FLAG_AUTOGROW)))
        throw parse_error (values, "Out of memory");
    for (auto &&val : values) {
        std::string ids = val.as<std::string> ();
        if (add_idset_string (ranks, ids.c_str ()) < 0) {
            idset_destroy (ranks);
            std::string msg = "Invalid idset `" + ids + "'";
            throw parse_error (val, msg.c_str ());
        }
    }
}

bool RankConstraint::match (const Flux::resource_model::resource_t &r) const
{
    return idset_test (ranks, r.rank);
}

YAML::Node RankConstraint::as_yaml () const
{
    YAML::Node node;
    char *s = idset_encode (ranks, IDSET_FLAG_RANGE);
    node["ranks"].push_back (s);
    free (s);
    return node;
}
