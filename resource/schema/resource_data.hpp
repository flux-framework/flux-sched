/*****************************************************************************\
 *  LLNL-CODE-658032 All rights reserved.
 *
 *  This file is part of the Flux resource manager framework.
 *  For details, see https://github.com/flux-framework.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license, or (at your option)
 *  any later version.
 *
 *  Flux is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *  See also:  http://www.gnu.org/licenses/
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
#include "resource/schema/data_std.hpp"
#include "resource/schema/sched_data.hpp"
#include "resource/schema/infra_data.hpp"
#include "resource/planner/planner.h"

namespace Flux {
namespace resource_model {

//! Resource pool data type
struct resource_pool_t {
    resource_pool_t ();
    resource_pool_t (const resource_pool_t &o);
    resource_pool_t &operator= (const resource_pool_t &o);
    ~resource_pool_t ();

    enum class status_t : int { 
        UP = 0,
        DOWN = 1
    };

    typedef std::unordered_map<std::string, status_t> string_to_status;
    static const string_to_status str_to_status;

    // Resource pool data
    std::string type;
    std::map<std::string, std::string> paths;
    std::string basename;
    std::string name;
    std::map<std::string, std::string> properties;
    int64_t id = -1;
    int64_t uniq_id;
    unsigned int size = 0;
    int rank = -1;
    std::string unit;

    schedule_t schedule;    //!< schedule data
    pool_infra_t idata;     //!< scheduling infrastructure data
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
    ~resource_relation_t ();

    std::map<std::string, std::string> name; //!< subsystem: relationship
    relation_infra_t idata; //!< scheduling infrastructure data
};

} // namespace resource_model
} // namespace Flux

#endif // RESOURCE_DATA_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
