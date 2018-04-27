/*****************************************************************************\
 *  Copyright (c) 2014 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
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

#ifndef DFU_MATCH_LOCALITY_HPP
#define DFU_MATCH_LOCALITY_HPP

#include <iostream>
#include <vector>
#include <numeric>
#include <map>
#include <boost/icl/interval.hpp>
#include <boost/icl/interval_set.hpp>
#include "dfu_match_cb.hpp"

namespace Flux {
namespace resource_model {

/*! Locality-aware policy: select resources of each type
 *  where you have more qualified.
 */
struct greater_interval_first_t : public dfu_match_cb_t
{
    greater_interval_first_t ();
    greater_interval_first_t (const std::string &name);
    greater_interval_first_t (const greater_interval_first_t &o);
    greater_interval_first_t &operator= (const greater_interval_first_t &o);
    ~greater_interval_first_t ();

    int dom_finish_graph (const subsystem_t &subsystem,
                          const std::vector<Flux::Jobspec::Resource> &resources,
                          const f_resource_graph_t &g, scoring_api_t &dfu);

    int dom_finish_vtx (vtx_t u, const subsystem_t &subsystem,
                        const std::vector<Flux::Jobspec::Resource> &resources,
                        const f_resource_graph_t &g, scoring_api_t &dfu);

    int dom_finish_slot (const subsystem_t &subsystem, scoring_api_t &dfu);
};

}
}

#endif //DFU_MATCH_LOCALITY_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
