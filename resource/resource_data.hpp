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
#include <set>
#include "planner/planner.h"

namespace Flux {
namespace resource_model {

// We create an x_checker planner for each resource vertex for quick exclusivity
// checking. We update x_checker for all of the vertices involved in each
// job allocation/reservation -- subtract 1 from x_checker planner for the
// scheduled span. Any vertex with less than X_CHECKER_NJOBS available in its
// x_checker cannot be exclusively allocated or reserved.
const char * const X_CHECKER_JOBS_STR = "jobs";
const int64_t X_CHECKER_NJOBS = 0x40000000;

typedef std::string subsystem_t;
typedef std::map<subsystem_t, std::string> multi_subsystems_t;
typedef std::map<subsystem_t, std::set<std::string>> multi_subsystemsS;

class color_t {
public:
    enum class color_offset_t : uint64_t {
        WHITE_OFFSET = 0,
        GRAY_OFFSET = 1,
        BLACK_OFFSET = 2,
        NEW_BASE = 3
    };

    void reset ()
    {
        m_color_base += static_cast<uint64_t>(color_offset_t::NEW_BASE);
    }
    bool is_white (uint64_t c) const
    {
        return c <= (m_color_base
                     + static_cast<uint64_t>(color_offset_t::WHITE_OFFSET));
    }
    uint64_t white () const
    {
        return m_color_base
               + static_cast<uint64_t>(color_offset_t::WHITE_OFFSET);
    }
    bool is_gray (uint64_t c) const
    {
        return c == (m_color_base
                     + static_cast<uint64_t>(color_offset_t::GRAY_OFFSET));
    }
    uint64_t gray () const
    {
        return m_color_base
               + static_cast<uint64_t>(color_offset_t::GRAY_OFFSET);
    }
    bool is_black (uint64_t c) const
    {
        return c == (m_color_base
                     + static_cast<uint64_t>(color_offset_t::BLACK_OFFSET));
    }
    uint64_t black () const
    {
        return m_color_base
               + static_cast<uint64_t>(color_offset_t::BLACK_OFFSET);
    }

private:
    uint64_t m_color_base = 0;
};

//! Type to keep track of current schedule state
struct schedule_t {
    schedule_t () { }
    schedule_t (const schedule_t &o)
    {
        int64_t base_time = 0;
        uint64_t duration = 0;
        size_t len = 0;

        // copy constructor does not copy the contents
        // of the schedule tables and of the planner objects.
        if (o.plans) {
            base_time = planner_base_time (o.plans);
            duration = planner_duration (o.plans);
            len = planner_resources_len (o.plans);
            plans = planner_new (base_time, duration,
                                 planner_resource_totals (o.plans),
                                 planner_resource_types (o.plans), len);
        }
        if (o.x_checker) {
            base_time = planner_base_time (o.x_checker);
            duration = planner_duration (o.x_checker);
            len = planner_resources_len (o.x_checker);
            x_checker = planner_new (base_time, duration,
                                     planner_resource_totals (o.x_checker),
                                     planner_resource_types (o.x_checker), len);
        }
    }
    schedule_t &operator= (const schedule_t &o)
    {
        int64_t base_time = 0;
        uint64_t duration = 0;
        size_t len = 0;

        // assign operator does not copy the contents
        // of the schedule tables and of the planner objects.
        if (o.plans) {
            base_time = planner_base_time (o.plans);
            duration = planner_duration (o.plans);
            len = planner_resources_len (o.plans);
            plans = planner_new (base_time, duration,
                                 planner_resource_totals (o.plans),
                                 planner_resource_types (o.plans), len);
        }
        if (o.x_checker) {
            base_time = planner_base_time (o.x_checker);
            duration = planner_duration (o.x_checker);
            len = planner_resources_len (o.x_checker);
            x_checker = planner_new (base_time, duration,
                                     planner_resource_totals (o.x_checker),
                                     planner_resource_types (o.x_checker), len);
        }
        return *this;
    }
    ~schedule_t ()
    {
        if (plans)
            planner_destroy (&plans);
        if (x_checker)
            planner_destroy (&x_checker);
    }

    std::map<int64_t, int64_t> tags;
    std::map<int64_t, int64_t> allocations;
    std::map<int64_t, int64_t> reservations;
    std::map<int64_t, int64_t> x_spans;
    planner_t *plans = NULL;
    planner_t *x_checker = NULL;
};

/*! Base type to organize the data supporting scheduling infrastructure's
 * operations (e.g., graph organization, coloring and edge evaluation).
 */
struct infra_base_t {
    infra_base_t () { }
    infra_base_t (const infra_base_t &o)
    {
        member_of = o.member_of;
    }
    infra_base_t &operator= (const infra_base_t &o)
    {
        member_of = o.member_of;
        return *this;
    }
    virtual ~infra_base_t () { }
    virtual void scrub () = 0;

    multi_subsystems_t member_of;
};

struct pool_infra_t : public infra_base_t {
    pool_infra_t () { }
    pool_infra_t (const pool_infra_t &o): infra_base_t (o)
    {
        // don't copy the content of infrastructure tables and subtree
        // planner objects.
        colors = o.colors;
        for (auto &kv : o.subplans) {
            planner_t *p = kv.second;
            if (!p)
                continue;
            int64_t base_time = planner_base_time (p);
            uint64_t duration = planner_duration (p);
            size_t len = planner_resources_len (p);
            subplans[kv.first] = planner_new (base_time, duration,
                                     planner_resource_totals (p),
                                     planner_resource_types (p), len);
        }
    }
    pool_infra_t &operator= (const pool_infra_t &o)
    {
        // don't copy the content of infrastructure tables and subtree
        // planner objects.
        infra_base_t::operator= (o);
        colors = o.colors;
        for (auto &kv : o.subplans) {
            planner_t *p = kv.second;
            if (!p)
                continue;
            int64_t base_time = planner_base_time (p);
            uint64_t duration = planner_duration (p);
            size_t len = planner_resources_len (p);
            subplans[kv.first] = planner_new (base_time, duration,
                                     planner_resource_totals (p),
                                     planner_resource_types (p), len);
        }
        return *this;
    }
    virtual ~pool_infra_t ()
    {
        for (auto &kv : subplans)
            planner_destroy (&(kv.second));
    }
    virtual void scrub ()
    {
        job2span.clear ();
        for (auto &kv : subplans)
            planner_destroy (&(kv.second));
        colors.clear ();
    }

    std::map<int64_t, int64_t> job2span;
    std::map<subsystem_t, planner_t *> subplans;
    std::map<subsystem_t, uint64_t> colors;
};

struct relation_infra_t : public infra_base_t {
    relation_infra_t () { }
    relation_infra_t (const relation_infra_t &o): infra_base_t (o)
    {
        needs = o.needs;
        best_k_cnt = o.best_k_cnt;
        exclusive = o.exclusive;
    }
    relation_infra_t &operator= (const relation_infra_t &o)
    {
        infra_base_t::operator= (o);
        needs = o.needs;
        best_k_cnt = o.best_k_cnt;
        exclusive = o.exclusive;
        return *this;
    }
    virtual ~relation_infra_t ()
    {

    }
    virtual void scrub ()
    {
        needs = 0;
        best_k_cnt = 0;
        exclusive = 0;
    }

    uint64_t needs = 0;
    uint64_t best_k_cnt = 0;
    int exclusive = 0;
};

//! Resource pool data type
struct resource_pool_t {
    resource_pool_t () { }
    resource_pool_t (const resource_pool_t &o)
    {
        type = o.type;
        paths = o.paths;
        basename = o.basename;
        name = o.name;
        properties = o.properties;
        id = o.id;
        memcpy (uuid, o.uuid, sizeof (uuid));
        size = o.size;
        unit = o.unit;
        schedule = o.schedule;
        idata = o.idata;
    }
    resource_pool_t &operator= (const resource_pool_t &o)
    {
        type = o.type;
        paths = o.paths;
        basename = o.basename;
        name = o.name;
        properties = o.properties;
        id = o.id;
        memcpy (uuid, o.uuid, sizeof (uuid));
        size = o.size;
        unit = o.unit;
        schedule = o.schedule;
        idata = o.idata;
        return *this;
    }
    ~resource_pool_t ()
    {
    }

    // Resource pool data
    std::string type;
    std::map<std::string, std::string> paths;
    std::string basename;
    std::string name;
    std::map<std::string, std::string> properties;
    int64_t id = -1;
    uuid_t uuid;
    unsigned int size = 0;
    std::string unit;

    schedule_t schedule;    //!< schedule data
    pool_infra_t idata;     //!< scheduling infrastructure data
};

/*! Resource relationship type.
 *  An edge is annotated with a set of {key: subsystem, val: relationship}.
 *  An edge can represent a relationship within a subsystem and do this
 *  for multiple subsystems.  However, it cannot represent multiple
 *  relationship within the same subsystem.
 */
struct resource_relation_t {
    resource_relation_t () { }
    resource_relation_t (const resource_relation_t &o)
    {
        name = o.name;
        idata = o.idata;
    }
    resource_relation_t &operator= (const resource_relation_t &o)
    {
        name = o.name;
        idata = o.idata;
        return *this;
    }
    ~resource_relation_t () { }

    std::string name;
    relation_infra_t idata; //!< scheduling infrastructure data
};

} // namespace resource_model
} // namespace Flux

#endif // RESOURCE_DATA_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
