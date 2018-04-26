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

#ifndef DATA_STD_H
#define DATA_STD_H

#include <set>
#include <string>

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

} // Flux
} // Flux::resource_model

#endif // DATA_STD_H

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
