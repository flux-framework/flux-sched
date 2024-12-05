/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef RESOURCE_DATA_HPP
#define RESOURCE_DATA_HPP

#include <uuid/uuid.h>
#include <string>
#include <cstring>
#include <map>
#include <unordered_map>
#include <set>
#include "resource/schema/color.hpp"
#include "resource/schema/resource_base.hpp"
#include "resource/schema/data_std.hpp"
#include "resource/schema/sched_data.hpp"
#include "resource/schema/infra_data.hpp"
#include "resource/planner/c/planner.h"

namespace Flux {
namespace resource_model {

//! Resource pool data type
struct resource_pool_t : public resource_t {
    resource_pool_t ();
    resource_pool_t (const resource_pool_t &o);
    resource_pool_t (resource_pool_t &&o);
    resource_pool_t &operator= (const resource_pool_t &o);
    resource_pool_t &operator= (resource_pool_t &&o);
    ~resource_pool_t ();

    enum class status_t : int { UP = 0, DOWN = 1 };

    typedef std::unordered_map<std::string, status_t> string_to_status;
    static const string_to_status str_to_status;
    static const std::string status_to_str (status_t s);

    // Resource pool data
    std::map<subsystem_t, std::string> paths;
    int64_t uniq_id;
    unsigned int size = 0;
    std::string unit;
    unsigned mps_data = 0;
    schedule_t schedule;  //!< schedule data
    pool_infra_t idata;   //!< scheduling infrastructure data
    status_t status = status_t::UP;
};

/*! Resource relationship type.
 *  An edge is annotated with a set of {key: subsystem, val: relationship}.
 *  An edge can represent a relationship within a subsystem and do this
 *  for multiple subsystems.  However, it cannot represent multiple
 *  relationship within the same subsystem.
 */
struct resource_relation_t {
    resource_relation_t ();
    resource_relation_t (const resource_relation_t &o);
    resource_relation_t &operator= (const resource_relation_t &o);
    resource_relation_t (resource_relation_t &&o);
    resource_relation_t &operator= (resource_relation_t &&o);
    ~resource_relation_t ();

    subsystem_t subsystem;   //!< subsystem this edge belongs to
    relation_infra_t idata;  //!< scheduling infrastructure data
};

}  // namespace resource_model
}  // namespace Flux

#endif  // RESOURCE_DATA_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
