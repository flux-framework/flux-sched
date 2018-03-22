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
#include <boost/icl/interval.hpp>
#include <boost/icl/interval_set.hpp>
#include "resource_graph.hpp"

namespace Flux {
namespace resource_model {

struct eval_edg_t {
    eval_edg_t (unsigned int c, unsigned int n, unsigned int x, edg_t e)
        : count (c), needs (n), exclusive (x), edge (e) { }
    eval_edg_t (unsigned int c, unsigned int n, unsigned int x)
        : count (c), needs (n), exclusive (x) { }
    unsigned int count = 0;
    unsigned int needs = 0;
    unsigned int exclusive = 0;
    edg_t edge;
};

struct eval_egroup_t {
    eval_egroup_t () { }
    eval_egroup_t (int64_t s, unsigned int c, unsigned int n,
                   unsigned int x, bool r)
        : score (s), count (c), needs (n), exclusive (x), root (r) {}
    eval_egroup_t (const eval_egroup_t &o)
    {
        score = o.score;
        count = o.count;
        needs = o.needs;
        exclusive = o.exclusive;
        root = o.root;
        edges = o.edges;
    }
    eval_egroup_t &operator= (const eval_egroup_t &o)
    {
        score = o.score;
        count = o.count;
        needs = o.needs;
        exclusive = o.exclusive;
        root = o.root;
        edges = o.edges;
        return *this;
    }

    int64_t score = -1;
    unsigned int count = 0;
    unsigned int needs = 0;
    unsigned int exclusive = 0;
    bool root = false;
    std::vector<eval_edg_t> edges;
};

namespace detail {
class evals_t {
public:
    evals_t () { }
    evals_t (int64_t cutline, const std::string &res_type)
        : m_resrc_type (res_type), m_cutline (cutline) { }
    evals_t (const std::string &res_type)
        : m_resrc_type (res_type) { }
    evals_t (const evals_t &o)
    {
        m_eval_egroups = o.m_eval_egroups;
        m_resrc_type = o.m_resrc_type;
        m_cutline = o.m_cutline;
        m_qual_count = o.m_qual_count;
        m_total_count = o.m_total_count;
        m_best_k = o.m_best_k;
        m_best_i = o.m_best_i;
    }
    ~evals_t ()
    {
        m_eval_egroups.clear ();
    }
    evals_t &operator= (const evals_t &o)
    {
        m_eval_egroups = o.m_eval_egroups;
        m_resrc_type = o.m_resrc_type;
        m_cutline = o.m_cutline;
        m_qual_count = o.m_qual_count;
        m_total_count = o.m_total_count;
        m_best_k = o.m_best_k;
        m_best_i = o.m_best_i;
        return *this;
    }

    unsigned int add (const eval_egroup_t &eg)
    {
        m_total_count += eg.count;
        if (eg.score > m_cutline)
            m_qual_count += eg.count;
        m_eval_egroups.push_back (eg);
        return m_qual_count;
    }

    // This can throw out_of_range exception
    const eval_egroup_t &at (unsigned int i) const
    {
        return m_eval_egroups.at (i);
    }

    unsigned int qualified_count () const
    {
        return m_qual_count;
    }

    unsigned int total_count () const
    {
        return m_total_count;
    }

    int64_t cutline () const
    {
        return m_cutline;
    }

    int64_t set_cutline (int64_t cutline)
    {
        int64_t rc = m_cutline;
        m_cutline = cutline;
        return rc;
    }

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
    int64_t accum_best_k (binary_op accum, int init=0)
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
        return std::transform (m_eval_egroups.begin (),
                               m_eval_egroups.end (),
                               o_it, uop);
    }

    unsigned int best_k () const
    {
        return m_best_k;
    }

    unsigned int best_i () const
    {
        return m_best_i;
    }

    int merge (evals_t &o)
    {
        if (m_cutline != o.m_cutline || m_resrc_type != o.m_resrc_type) {
            errno = EINVAL;
            return -1;
        }
        m_qual_count += o.m_qual_count;
        m_total_count += o.m_total_count;
        m_cutline = o.m_cutline;
        m_eval_egroups.insert (m_eval_egroups.end (), o.m_eval_egroups.begin (),
                               o.m_eval_egroups.end ());
        return 0;
    }

    void rewind_iter_cur ()
    {
        iter_cur = m_eval_egroups.begin ();
    }

    std::vector<eval_egroup_t>::iterator iter_cur;

private:
    std::vector<eval_egroup_t> m_eval_egroups;
    std::string m_resrc_type;
    int64_t m_cutline = 0;
    unsigned int m_qual_count = 0;
    unsigned int m_total_count = 0;
    unsigned int m_best_k = 0;      //<! best-k to be selected
    unsigned int m_best_i = 0;      //<! first i elements in has best-k
};
} // namespace detail

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

class scoring_api_t {
public:
    scoring_api_t () {}
    scoring_api_t (const scoring_api_t &o)
    {
        for (auto &p : o.m_ssys_map) {
            const subsystem_t &s = p.first;
            auto obj = new std::map<const std::string, detail::evals_t *>();
            m_ssys_map.insert (std::make_pair (s, obj));
            auto &tmap = *(p.second);
            for (auto &p2 : tmap) {
                const std::string &res_type = p2.first;
                detail::evals_t *ne = new detail::evals_t ();
                *ne = *(p2.second);
                (*m_ssys_map[s]).insert (std::make_pair (res_type, ne));
            }
        }
    }
    const scoring_api_t &operator= (const scoring_api_t &o)
    {
        for (auto &p : o.m_ssys_map) {
            const subsystem_t &s = p.first;
            auto obj = new std::map<const std::string, detail::evals_t *>();
            m_ssys_map.insert (std::make_pair (s, obj));
            auto &tmap = *(p.second);
            for (auto &p2 : tmap) {
                const std::string &res_type = p2.first;
                detail::evals_t *ne = new detail::evals_t ();
                *ne = *(p2.second);
                (*m_ssys_map[s]).insert (std::make_pair (res_type, ne));
            }
        }
        return *this;
    }

    ~scoring_api_t ()
    {
        auto i = m_ssys_map.begin ();
        while (i != m_ssys_map.end ()) {
            auto tmap = i->second;
            auto j = tmap->begin ();
            while (j != tmap->end ()) {
                delete j->second;
                j = tmap->erase (j);
            }
            delete i->second;
            i = m_ssys_map.erase (i);
        }
    }

    int64_t cutline (const subsystem_t &s, const std::string &r)
    {
        handle_new_keys (s, r);
        auto res_evals = (*m_ssys_map[s])[r];
        return res_evals->cutline ();
    }

    int64_t set_cutline (const subsystem_t &s, const std::string &r,
                         int64_t c)
    {
        handle_new_keys (s, r);
        auto res_evals = (*m_ssys_map[s])[r];
        return res_evals->set_cutline (c);
    }

    void rewind_iter_cur (const subsystem_t &s, const std::string &r)
    {
        handle_new_keys (s, r);
        auto res_evals = (*m_ssys_map[s])[r];
        return res_evals->rewind_iter_cur ();
    }

    std::vector<eval_egroup_t>::iterator iter_cur (const subsystem_t &s,
                                                   const std::string &r)
    {
        handle_new_keys (s, r);
        auto res_evals = (*m_ssys_map[s])[r];
        return res_evals->iter_cur;
    }

    void incr_iter_cur (const subsystem_t &s, const std::string &r)
    {
        handle_new_keys (s, r);
        auto res_evals = (*m_ssys_map[s])[r];
        res_evals->iter_cur++;
    }

    int add (const subsystem_t &s, const std::string &r, const eval_egroup_t &eg)
    {
        handle_new_keys (s, r);
        auto res_evals = (*m_ssys_map[s])[r];
        return res_evals->add (eg);
    }

    //! Can throw an out_of_range exception
    const eval_egroup_t &at (const subsystem_t &s, const std::string &r,
              unsigned int i)
    {
        handle_new_keys (s, r);
        auto res_evals = (*m_ssys_map[s])[r];
        return res_evals->at(i);
    }

    unsigned int qualified_count (const subsystem_t &s, const std::string &r)
    {
        handle_new_keys (s, r);
        auto res_evals = (*m_ssys_map[s])[r];
        return res_evals->qualified_count ();
    }

    unsigned int total_count (const subsystem_t &s, const std::string &r)
    {
        handle_new_keys (s, r);
        auto res_evals = (*m_ssys_map[s])[r];
        return res_evals->total_count ();
    }

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

    unsigned int best_k (const subsystem_t &s, const std::string &r)
    {
        handle_new_keys (s, r);
        auto res_evals = (*m_ssys_map[s])[r];
        return res_evals->best_k ();
    }

    unsigned int best_i (const subsystem_t &s, const std::string &r)
    {
        handle_new_keys (s, r);
        auto res_evals = (*m_ssys_map[s])[r];
        return res_evals->best_i ();
    }

    bool hier_constrain_now ()
    {
        return m_hier_constrain_now;
    }

    void merge (const scoring_api_t &o)
    {
        for (auto &kv : o.m_ssys_map) {
            const subsystem_t &s = kv.first;
            auto &tmap = *(kv.second);
            for (auto &kv2 : tmap) {
                const std::string &r = kv2.first;
                auto &ev = *(kv2.second);
                handle_new_keys (s, r);
                auto res_evals = (*m_ssys_map[s])[r];
                res_evals->merge (ev);
            }
        }
    }

    void resrc_types (const subsystem_t &s, std::vector<std::string> &v)
    {
        handle_new_subsystem (s);
        for (auto &kv : *(m_ssys_map[s]))
            v.push_back (kv.first);
    }

    // overall_score and avail are temporary space such that
    // a child vertex visitor can pass the info to the parent vertex
    int64_t overall_score ()
    {
        return m_overall_score;
    }

    void set_overall_score (int64_t overall)
    {
        m_overall_score = overall;
    }

    unsigned int avail ()
    {
        return m_avail;
    }

    void set_avail (unsigned int avail)
    {
        m_avail = avail;
    }

private:
    void handle_new_keys (const subsystem_t &s, const std::string &r)
    {
        handle_new_subsystem (s);
        handle_new_resrc_type (s, r);
    }

    void handle_new_subsystem (const subsystem_t &s)
    {
        if (m_ssys_map.find (s) == m_ssys_map.end ()) {
            auto o = new std::map<const std::string, detail::evals_t *>();
            m_ssys_map.insert (std::make_pair (s, o));
        }
    }

    void handle_new_resrc_type (const subsystem_t &s, const std::string &r)
    {
        if (m_ssys_map[s]->find (r) == m_ssys_map[s]->end ()) {
            auto e = new detail::evals_t (r);
            m_ssys_map[s]->insert (std::make_pair (r, e));
        }
    }

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
