/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
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
    inline bool operator() (const eval_egroup_t &a, const eval_egroup_t &b) const
    {
        return a.score > b.score;
    }
};

struct less {
    inline bool operator() (const eval_egroup_t &a, const eval_egroup_t &b) const
    {
        return a.score < b.score;
    }
};

struct interval_greater {
    inline bool operator() (const eval_egroup_t &a, const eval_egroup_t &b) const
    {
        return *(ivset.find (a.score)) > *(ivset.find (b.score));
    }
    boost::icl::interval_set<int64_t> ivset;
};

struct interval_less {
    inline bool operator() (const eval_egroup_t &a, const eval_egroup_t &b) const
    {
        return *(ivset.find (a.score)) < *(ivset.find (b.score));
    }
    boost::icl::interval_set<int64_t> ivset;
};

struct plus {
    inline const int64_t operator() (const int64_t &result, const eval_egroup_t &a) const
    {
        return result + a.score;
    }
};
inline boost::icl::interval_set<int64_t>::interval_type to_interval (const eval_egroup_t &ev)
{
    using namespace boost::icl;
    int64_t tmp = ev.score;
    return interval_set<int64_t>::interval_type::closed (tmp, tmp);
}
}  // namespace fold

}  // namespace resource_model
}  // namespace Flux

#endif  // FOLD_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
