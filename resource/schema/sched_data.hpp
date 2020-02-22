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

#ifndef SCHED_DATA_H
#define SCHED_DATA_H

#include <map>
#include <cstdint>
#include "resource/planner/planner.h"

namespace Flux {
namespace resource_model {

//! Type to keep track of current schedule state
struct schedule_t {
    schedule_t ();
    schedule_t (const schedule_t &o);
    schedule_t &operator= (const schedule_t &o);
    ~schedule_t ();

    std::map<int64_t, int64_t> allocations;
    std::map<int64_t, int64_t> reservations;
    planner_t *plans = NULL;
    bool elastic_job = false;
    bool elastic_job_running_at = false;
    int64_t elastic_at = -1;
    uint64_t elastic_duration = 0;
};

} // Flux::resource_model
} // Flux

#endif // SCHED_DATA_H

