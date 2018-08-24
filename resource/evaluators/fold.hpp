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

#ifndef FOLD_HPP
#define FOLD_HPP

#include <boost/icl/interval.hpp>
#include <boost/icl/interval_set.hpp>
#include "resource/evaluators/edge_eval_api.hpp"

namespace Flux {
namespace resource_model {

namespace fold {
struct greater {
    bool operator() (const eval_egroup_t &a, const eval_egroup_t &b) const
    {
        return a.score > b.score;
    }
};

struct less {
    bool operator() (const eval_egroup_t &a, const eval_egroup_t &b) const
    {
        return a.score < b.score;
    }
};

struct interval_greater {
    bool operator() (const eval_egroup_t &a, const eval_egroup_t &b) const
    {
        return *(ivset.find (a.score)) > *(ivset.find (b.score));
    }
    boost::icl::interval_set<int64_t> ivset;
};

struct interval_less {
    bool operator() (const eval_egroup_t &a, const eval_egroup_t &b) const
    {
        return *(ivset.find (a.score)) < *(ivset.find (b.score));
    }
    boost::icl::interval_set<int64_t> ivset;
};

struct plus {
    const int64_t operator() (const int64_t result,
                              const eval_egroup_t &a) const
    {
        return result + a.score;
    }
};
inline boost::icl::interval_set<int64_t>::interval_type to_interval (
                                                     const eval_egroup_t &ev)
{
    using namespace boost::icl;
    int64_t tmp = ev.score;
    return interval_set<int64_t>::interval_type::closed (tmp, tmp);
}
} // namespace fold

} // namespace resource_model
} // namesapce Flux

#endif // FOLD_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
