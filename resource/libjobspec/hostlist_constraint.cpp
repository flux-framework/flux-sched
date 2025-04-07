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

#include "hostlist_constraint.hpp"

using namespace Flux::Jobspec;

HostlistConstraint::HostlistConstraint (const YAML::Node &values)
{
    if (!(hl = hostlist_create ()))
        throw parse_error (values, "Out of memory");
    for (auto &&val : values) {
        std::string hosts = val.as<std::string> ();
        if (hostlist_append (hl, hosts.c_str ()) < 0) {
            hostlist_destroy (hl);
            std::string msg = "Invalid hostlist `" + hosts + "'";
            throw parse_error (val, msg.c_str ());
        }
    }
}

bool HostlistConstraint::match (const Flux::resource_model::resource_t &r) const
{
    int saved_errno = errno;
    int rc = hostlist_find (hl, r.name.c_str ());
    errno = saved_errno;
    return rc < 0 ? false : true;
}

YAML::Node HostlistConstraint::as_yaml () const
{
    YAML::Node node;
    char *hosts = hostlist_encode (hl);
    node["hostlist"].push_back (hosts);
    free (hosts);
    return node;
}
