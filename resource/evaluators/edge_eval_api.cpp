/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

#include "resource/evaluators/edge_eval_api.hpp"

namespace Flux {
namespace resource_model {

////////////////////////////////////////////////////////////////////////////////
// Edge Evaluator Public Method Definitions
////////////////////////////////////////////////////////////////////////////////

eval_edg_t::eval_edg_t (unsigned int c, unsigned int n, unsigned int x, edg_t e)
    : count (c), needs (n), exclusive (x), edge (e)
{
}

eval_edg_t::eval_edg_t (unsigned int c, unsigned int n, unsigned int x)
    : count (c), needs (n), exclusive (x)
{
}

eval_egroup_t::eval_egroup_t ()
{
}

eval_egroup_t::eval_egroup_t (int64_t s, unsigned int c, unsigned int n, unsigned int x, bool r)
    : score (s), count (c), needs (n), exclusive (x), root (r)
{
}

eval_egroup_t::eval_egroup_t (const eval_egroup_t &o)
{
    score = o.score;
    count = o.count;
    needs = o.needs;
    exclusive = o.exclusive;
    root = o.root;
    edges = o.edges;
}

eval_egroup_t &eval_egroup_t::operator= (const eval_egroup_t &o)
{
    score = o.score;
    count = o.count;
    needs = o.needs;
    exclusive = o.exclusive;
    root = o.root;
    edges = o.edges;
    return *this;
}

namespace detail {

evals_t::evals_t ()
{
}

evals_t::evals_t (int64_t cutline, resource_type_t res_type)
    : m_resrc_type (res_type), m_cutline (cutline)
{
}

evals_t::evals_t (resource_type_t res_type) : m_resrc_type (res_type)
{
}

evals_t::evals_t (const evals_t &o)
{
    m_eval_egroups = o.m_eval_egroups;
    m_resrc_type = o.m_resrc_type;
    m_cutline = o.m_cutline;
    m_qual_count = o.m_qual_count;
    m_total_count = o.m_total_count;
    m_best_k = o.m_best_k;
    m_best_i = o.m_best_i;
}

evals_t::~evals_t ()
{
    m_eval_egroups.clear ();
}

evals_t &evals_t::operator= (const evals_t &o)
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

unsigned int evals_t::add (const eval_egroup_t &eg)
{
    m_total_count += eg.count;
    if (eg.score > m_cutline)
        m_qual_count += eg.count;
    m_eval_egroups.push_back (eg);
    return m_qual_count;
}

const eval_egroup_t &evals_t::at (unsigned int i) const
{
    return m_eval_egroups.at (i);
}

unsigned int evals_t::qualified_count () const
{
    return m_qual_count;
}

unsigned int evals_t::qualified_granules () const
{
    return m_eval_egroups.size ();
}

unsigned int evals_t::total_count () const
{
    return m_total_count;
}

int64_t evals_t::cutline () const
{
    return m_cutline;
}

int64_t evals_t::set_cutline (int64_t cutline)
{
    int64_t rc = m_cutline;
    m_cutline = cutline;
    return rc;
}

unsigned int evals_t::best_k () const
{
    return m_best_k;
}

unsigned int evals_t::best_i () const
{
    return m_best_i;
}

int evals_t::merge (const evals_t &o)
{
    if (m_cutline != o.m_cutline || m_resrc_type != o.m_resrc_type) {
        errno = EINVAL;
        return -1;
    }
    m_qual_count += o.m_qual_count;
    m_total_count += o.m_total_count;
    m_cutline = o.m_cutline;
    m_eval_egroups.insert (m_eval_egroups.end (),
                           o.m_eval_egroups.begin (),
                           o.m_eval_egroups.end ());
    return 0;
}

void evals_t::eval_egroups_iter_reset ()
{
    m_iter_cur_reset = true;
}

std::vector<eval_egroup_t>::iterator evals_t::eval_egroups_iter_next ()
{
    if (m_iter_cur_reset) {
        m_iter_cur = m_eval_egroups.begin ();
        m_iter_cur_reset = false;
    } else if (m_iter_cur != m_eval_egroups.end ()) {
        m_iter_cur++;
    }
    return m_iter_cur;
}

std::vector<eval_egroup_t>::iterator evals_t::eval_egroups_end ()
{
    return m_eval_egroups.end ();
}

}  // namespace detail
}  // namespace resource_model
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
