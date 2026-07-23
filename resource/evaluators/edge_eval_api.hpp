/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef EDGE_EVAL_API_HPP
#define EDGE_EVAL_API_HPP

#include <vector>
#include <string>
#include "resource/schema/resource_graph.hpp"

namespace Flux {
namespace resource_model {

struct eval_edg_t {
    eval_edg_t (unsigned int c, unsigned int n, unsigned int x, edg_t e);
    eval_edg_t (unsigned int c, unsigned int n, unsigned int x);
    // compiler will generate a correct copy constructor

    unsigned int count = 0;
    unsigned int needs = 0;
    unsigned int exclusive = 0;
    // Resolved POOLED classification: true when a non-exclusive request
    // draws capacity from this edge's target vertex pool (the vertex has a
    // unit). Resolved once at edge-evaluation time from the jobspec request
    // and the matched vertex so that scoring, slot counting, binding, and
    // enforcement all share one authoritative predicate.
    bool pooled = false;
    edg_t edge;
};

struct eval_egroup_t {
    eval_egroup_t ();
    eval_egroup_t (int64_t s, unsigned int c, unsigned int n, unsigned int x, bool r);
    eval_egroup_t (const eval_egroup_t &o);
    eval_egroup_t &operator= (const eval_egroup_t &o);

    int64_t score = -1;
    unsigned int count = 0;
    unsigned int needs = 0;
    unsigned int exclusive = 0;
    bool root = false;
    std::vector<eval_edg_t> edges;
};

namespace detail {

/* Per-(subsystem, type) collection of evaluated edge groups.
 *
 * Invariant: m_eval_egroups is the single candidate set. add () appends
 * every egroup unconditionally -- m_qual_count/m_total_count are counters,
 * not filters, and nothing is partitioned by cutline. Consequently every
 * reader of the collection (pooled_shares (), the egroup iterator used for
 * slot packing and pooled binding, choose_best_k (), transform ()) walks
 * the same egroups: a count derived here can always be bound from here.
 */
class evals_t {
   public:
    evals_t ();
    evals_t (int64_t cutline, resource_type_t res_type);
    evals_t (resource_type_t res_type);
    evals_t (const evals_t &o);
    ~evals_t ();
    evals_t &operator= (const evals_t &o);

    unsigned int add (const eval_egroup_t &eg);
    // This can throw out_of_range exception
    const eval_egroup_t &at (unsigned int i) const;
    unsigned int qualified_count () const;
    unsigned int qualified_granules () const;
    unsigned int total_count () const;
    /* Shares backed by POOLED egroups: the sum over egroups whose edge is
     * pooled (see eval_edg_t::pooled) of how many whole per_share chunks
     * that egroup's single vertex can supply.  One share must come from ONE
     * vertex, so this is the authoritative share count for pooled resources;
     * capacity summed across vertices (total_count () / per_share) can
     * fabricate shares no single vertex can back.  Does not touch the
     * egroup iterator cursor.
     *
     * \return -1 when no pooled egroup exists (or per_share is 0); the
     *         per-vertex whole-share sum otherwise.
     */
    int64_t pooled_shares (unsigned int per_share) const;
    /* Incremental equivalent of pooled_shares () for callers that observe
     * the egroup vector as it grows (e.g. the exploration tally, which
     * updates after every add ()): accumulate whole per_share fits only
     * from egroups appended since the previous call, resuming at index
     * cursor and advancing it.  An index stays valid across add () and
     * merge () because both append; no reordering happens until a match
     * policy sorts the vector, after exploration completes.  shares is
     * left untouched (-1 by convention) until the first pooled egroup is
     * seen, preserving pooled_shares ()'s "no pooled egroups" signal.
     * Scanning every egroup exactly once makes the exploration-long cost
     * linear where repeated pooled_shares () rescans would be quadratic.
     */
    void pooled_shares_incr (unsigned int per_share, unsigned int &cursor, int64_t &shares) const;
    int64_t cutline () const;
    int64_t set_cutline (int64_t cutline);
    unsigned int best_k () const;
    unsigned int best_i () const;
    int merge (const evals_t &o);
    void eval_egroups_iter_reset ();
    std::vector<eval_egroup_t>::iterator eval_egroups_iter_next ();
    std::vector<eval_egroup_t>::iterator eval_egroups_end ();

    template<class compare_op>
    int choose_best_k (unsigned int k, compare_op comp)
    {
        if (k == 0 || k > m_qual_count) {
            errno = EINVAL;
            return -1;
        }

        unsigned int i = 0;
        int old = (int)m_best_k;
        int to_be_selected = k;
        std::sort (m_eval_egroups.begin (), m_eval_egroups.end (), comp);
        while (to_be_selected > 0) {
            if (to_be_selected <= (int)m_eval_egroups[i].count)
                m_eval_egroups[i].needs = to_be_selected;
            else
                m_eval_egroups[i].needs = m_eval_egroups[i].count;

            to_be_selected -= m_eval_egroups[i].count;
            i++;
        }
        m_best_k = k;
        m_best_i = i;
        return old;
    }

    template<class binary_op>
    int64_t accum_best_k (binary_op accum, int init = 0)
    {
        if (m_best_k == 0 || m_best_i == 0) {
            errno = EINVAL;
            return -1;
        }
        int64_t score_accum = init;
        for (unsigned int i = 0; i < m_best_i; i++)
            score_accum = accum (score_accum, m_eval_egroups[i]);
        return score_accum;
    }

    template<class output_it, class unary_op>
    output_it transform (output_it o_it, unary_op uop)
    {
        return std::transform (m_eval_egroups.begin (), m_eval_egroups.end (), o_it, uop);
    }

   private:
    std::vector<eval_egroup_t> m_eval_egroups;
    std::vector<eval_egroup_t>::iterator m_iter_cur;
    bool m_iter_cur_reset = true;
    resource_type_t m_resrc_type;
    int64_t m_cutline = 0;
    unsigned int m_qual_count = 0;
    unsigned int m_total_count = 0;
    unsigned int m_best_k = 0;  //<! best-k to be selected
    unsigned int m_best_i = 0;  //<! first i elements in has best-k
};

}  // namespace detail

}  // namespace resource_model
}  // namespace Flux

#endif  // EDGE_EVAL_API_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
