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

resource_pool_t::resource_pool_t ()
{
}

resource_pool_t::resource_pool_t (const resource_pool_t &o)
{
    type = o.type;
    paths = o.paths;
    basename = o.basename;
    name = o.name;
    properties = o.properties;
    id = o.id;
    uniq_id = o.uniq_id;
    rank = o.rank;
    status = o.status;
    size = o.size;
    unit = o.unit;
    schedule = o.schedule;
    idata = o.idata;
}

resource_pool_t &resource_pool_t::operator= (const resource_pool_t &o)
{
    type = o.type;
    paths = o.paths;
    basename = o.basename;
    name = o.name;
    properties = o.properties;
    id = o.id;
    uniq_id = o.uniq_id;
    rank = o.rank;
    status = o.status;
    size = o.size;
    unit = o.unit;
    schedule = o.schedule;
    idata = o.idata;
    return *this;
}

resource_pool_t::~resource_pool_t ()
{
}

////////////////////////////////////////////////////////////////////////////////
// Resource Relation Method Definitions
////////////////////////////////////////////////////////////////////////////////

resource_relation_t::resource_relation_t ()
{
}

resource_relation_t::resource_relation_t (const resource_relation_t &o)
{
    name = o.name;
    idata = o.idata;
}

resource_relation_t &resource_relation_t::operator= (const resource_relation_t &o)
{
    name = o.name;
    idata = o.idata;
    return *this;
}

resource_relation_t::~resource_relation_t ()
{
}

const resource_pool_t::string_to_status resource_pool_t::str_to_status =
    {{"up", resource_pool_t::status_t::UP}, {"down", resource_pool_t::status_t::DOWN}};

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
