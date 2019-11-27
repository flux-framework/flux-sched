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

#ifndef SCORING_API_HPP
#define SCORING_API_HPP

#include <map>
#include <vector>
#include <cstdint>
#include <cerrno>
#include <numeric>
#include <functional>
#include <algorithm>
#include "resource/schema/resource_graph.hpp"
#include "resource/evaluators/edge_eval_api.hpp"
#include "resource/evaluators/fold.hpp"

namespace Flux {
namespace resource_model {

class scoring_api_t {
public:
    scoring_api_t ();
    scoring_api_t (const scoring_api_t &o);
    const scoring_api_t &operator= (const scoring_api_t &o);
    ~scoring_api_t ();

    int64_t cutline (const subsystem_t &s, const std::string &r);
    int64_t set_cutline (const subsystem_t &s, const std::string &r, int64_t c);
    void rewind_iter_cur (const subsystem_t &s, const std::string &r);
    std::vector<eval_egroup_t>::iterator iter_cur (const subsystem_t &s,
                                                   const std::string &r);
    void incr_iter_cur (const subsystem_t &s, const std::string &r);
    int add (const subsystem_t &s, const std::string &r,
             const eval_egroup_t &eg);
    //! Can throw an out_of_range exception
    const eval_egroup_t &at (const subsystem_t &s, const std::string &r,
                             unsigned int i);
    unsigned int qualified_count (const subsystem_t &s, const std::string &r);
    unsigned int qualified_granules (const subsystem_t &s, const std::string &r);
    unsigned int total_count (const subsystem_t &s, const std::string &r);
    unsigned int best_k (const subsystem_t &s, const std::string &r);
    unsigned int best_i (const subsystem_t &s, const std::string &r);
    bool hier_constrain_now ();
    void merge (const scoring_api_t &o);
    void resrc_types (const subsystem_t &s, std::vector<std::string> &v);
    int64_t overall_score ();
    void set_overall_score (int64_t overall);
    unsigned int avail ();
    void set_avail (unsigned int avail);

    template<class compare_op = fold::greater, class binary_op = fold::plus>
    int64_t choose_accum_best_k (const subsystem_t &s, const std::string &r,
                                 unsigned int k,
                                 compare_op comp = fold::greater(),
                                 binary_op accum = fold::plus ())
    {
        int64_t rc;
        handle_new_keys (s, r);
        auto res_evals = (*m_ssys_map[s])[r];
        if ( (rc = res_evals->choose_best_k<compare_op> (k, comp)) != -1) {
            m_hier_constrain_now = true;
            rc = res_evals->accum_best_k<binary_op> (accum);
        }
        return rc;
    }

    template<class compare_op = fold::greater, class binary_op = fold::plus>
    int64_t choose_accum_all (const subsystem_t &s, const std::string &r,
                              compare_op comp = fold::greater (),
                              binary_op accum = fold::plus ())
    {
        int64_t rc;
        handle_new_keys (s, r);
        auto res_evals = (*m_ssys_map[s])[r];
        unsigned int k = res_evals->qualified_count ();
        if ( (rc = res_evals->choose_best_k<compare_op> (k, comp)) != -1) {
            m_hier_constrain_now = true;
            rc = res_evals->accum_best_k<binary_op> (accum);
        }
        return rc;
    }

    template<class output_it, class unary_op>
    output_it transform (const subsystem_t &s, const std::string &r,
                         output_it o_it, unary_op uop)
    {
        handle_new_keys (s, r);
        auto res_evals = (*m_ssys_map[s])[r];
        return res_evals->transform<output_it, unary_op> (o_it, uop);
    }

private:
    void handle_new_keys (const subsystem_t &s, const std::string &r);
    void handle_new_subsystem (const subsystem_t &s);
    void handle_new_resrc_type (const subsystem_t &s, const std::string &r);

    std::map<const subsystem_t,
             std::map<const std::string, detail::evals_t *> *> m_ssys_map;
    bool m_hier_constrain_now = false;
    int64_t m_overall_score = -1;
    unsigned int m_avail = 0;
};

} // namespace resource_model
} // namespace Flux

#endif // SCORING_API_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
