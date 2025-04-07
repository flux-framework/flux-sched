/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
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
    scoring_api_t (scoring_api_t &&o);
    scoring_api_t &operator= (const scoring_api_t &o);
    scoring_api_t &operator= (scoring_api_t &&o);
    ~scoring_api_t ();

    int64_t cutline (subsystem_t s, resource_type_t r);
    int64_t set_cutline (subsystem_t s, resource_type_t r, int64_t c);

    void eval_egroups_iter_reset (subsystem_t s, resource_type_t r);
    std::vector<eval_egroup_t>::iterator eval_egroups_iter_next (subsystem_t s, resource_type_t r);
    std::vector<eval_egroup_t>::iterator eval_egroups_end (subsystem_t s, resource_type_t r);

    int add (subsystem_t s, resource_type_t r, const eval_egroup_t &eg);
    //! Can throw an out_of_range exception
    const eval_egroup_t &at (subsystem_t s, resource_type_t r, unsigned int i);
    unsigned int qualified_count (subsystem_t s, resource_type_t r);
    unsigned int qualified_granules (subsystem_t s, resource_type_t r);
    unsigned int total_count (subsystem_t s, resource_type_t r);
    unsigned int best_k (subsystem_t s, resource_type_t r);
    unsigned int best_i (subsystem_t s, resource_type_t r);
    bool hier_constrain_now ();
    void merge (const scoring_api_t &o);
    void resrc_types (subsystem_t s, std::vector<resource_type_t> &v);
    int64_t overall_score ();
    void set_overall_score (int64_t overall);
    unsigned int avail ();
    void set_avail (unsigned int avail);
    bool is_contained (subsystem_t s, resource_type_t const &r);

    template<class compare_op = fold::greater, class binary_op = fold::plus>
    int64_t choose_accum_best_k (subsystem_t s,
                                 resource_type_t r,
                                 unsigned int k,
                                 compare_op comp = fold::greater (),
                                 binary_op accum = fold::plus ())
    {
        int64_t rc;
        auto &res_evals = m_ssys_map[s][r];
        if ((rc = res_evals.choose_best_k<compare_op> (k, comp)) != -1) {
            m_hier_constrain_now = true;
            rc = res_evals.accum_best_k<binary_op> (accum);
        }
        return rc;
    }

    template<class compare_op = fold::greater, class binary_op = fold::plus>
    int64_t choose_accum_all (subsystem_t s,
                              resource_type_t r,
                              compare_op comp = fold::greater (),
                              binary_op accum = fold::plus ())
    {
        int64_t rc;
        auto &res_evals = m_ssys_map[s][r];
        unsigned int k = res_evals.qualified_count ();
        if ((rc = res_evals.choose_best_k<compare_op> (k, comp)) != -1) {
            m_hier_constrain_now = true;
            rc = res_evals.accum_best_k<binary_op> (accum);
        }
        return rc;
    }

    template<class output_it, class unary_op>
    output_it transform (subsystem_t s, resource_type_t r, output_it o_it, unary_op uop)
    {
        auto &res_evals = m_ssys_map[s][r];
        return res_evals.transform<output_it, unary_op> (o_it, uop);
    }

   private:
    intern::interned_key_vec<subsystem_t, std::map<resource_type_t, detail::evals_t>> m_ssys_map;
    bool m_hier_constrain_now = false;
    int64_t m_overall_score = -1;
    unsigned int m_avail = 0;
};

}  // namespace resource_model
}  // namespace Flux

#endif  // SCORING_API_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
