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

#include "resource/schema/sched_data.hpp"

namespace Flux {
namespace resource_model {

schedule_t::schedule_t ()
{

}

schedule_t::schedule_t (const schedule_t &o)
{
    int64_t base_time = 0;
    uint64_t duration = 0;

    // copy constructor does not copy the contents
    // of the schedule tables and of the planner objects.
    if (o.plans) {
        base_time = planner_base_time (o.plans);
        duration = planner_duration (o.plans);
        plans = planner_new (base_time, duration,
                             planner_resource_total (o.plans),
                             planner_resource_type (o.plans));
    }
}

schedule_t &schedule_t::operator= (const schedule_t &o)
{
    int64_t base_time = 0;
    uint64_t duration = 0;

    // assign operator does not copy the contents
    // of the schedule tables and of the planner objects.
    if (o.plans) {
        base_time = planner_base_time (o.plans);
        duration = planner_duration (o.plans);
        plans = planner_new (base_time, duration,
                             planner_resource_total (o.plans),
                             planner_resource_type (o.plans));
    }
    return *this;
}

schedule_t::~schedule_t ()
{
    if (plans)
        planner_destroy (&plans);
}

} // resource_model
} // Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
