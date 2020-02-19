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
#include <set>
#include <cstdint>
#include "resource/planner/planner.h"

namespace Flux {
namespace resource_model {

struct spantype_t {
    int64_t span;
    std::string jobtype;
};

struct allotment_t {
    allotment_t ();
    void insert (const int64_t jobid, const int64_t span,
                 const std::string &jobtype);
    void erase (const int64_t jobid);
    ~allotment_t ();

    std::map<int64_t, spantype_t> id2spantype;
    std::map<std::string, std::set<int64_t>> type2id;
};

//! Type to keep track of current schedule state
struct schedule_t {
    schedule_t ();
    schedule_t (const schedule_t &o);
    schedule_t &operator= (const schedule_t &o);
    ~schedule_t ();

    allotment_t allocations;
    allotment_t reservations;
    planner_t *plans = NULL;
};

} // Flux::resource_model
} // Flux

#endif // SCHED_DATA_H

