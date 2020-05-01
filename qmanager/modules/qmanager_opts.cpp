/*****************************************************************************\
 *  Copyright (c) 2020 Lawrence Livermore National Security, LLC.  Produced at
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

#include "qmanager_opts.hpp"

using namespace Flux;
using namespace Flux::opts_manager;


/******************************************************************************
 *                                                                            *
 *              Private API for Queue Manager Option Class                    *
 *                                                                            *
 ******************************************************************************/

qmanager_opts_t &qmanager_opts_t::canonicalize ()
{
    // Placeholder: If there are options whose default values are
    // determined by another option, this method must
    // cannoicalize the state.
    return *this;
}

bool qmanager_opts_t::known_queue_policy (const std::string &policy)
{
    bool rc = false;
    if (policy == "fcfs" || policy == "easy"
        || policy == "hybrid" || policy == "conservative")
        rc = true;
    return rc;
}


/******************************************************************************
 *                                                                            *
 *                Private API for Queue Property Class                        *
 *                                                                            *
 ******************************************************************************/

bool queue_prop_t::known_queue_policy (const std::string &policy)
{
    bool rc = false;
    if (policy == "fcfs" || policy == "easy"
        || policy == "hybrid" || policy == "conservative")
        rc = true;
    return rc;
}


/******************************************************************************
 *                                                                            *
 *              Public API for Queue Manager Option Class                     *
 *                                                                            *
 ******************************************************************************/

const std::string &queue_prop_t::get_queue_policy () const
{
    return queue_policy;
}

const std::string &queue_prop_t::get_queue_params () const
{
    return queue_params;
}

const std::string &queue_prop_t::get_policy_params () const
{
    return policy_params;
}

bool queue_prop_t::set_queue_policy (const std::string &p)
{
    if (!known_queue_policy (p))
        return false;
    queue_policy = p;
    return true;
}

void queue_prop_t::set_queue_params (const std::string &p)
{
    queue_params = p;
}

void queue_prop_t::set_policy_params (const std::string &p)
{
    policy_params = p;
}

bool queue_prop_t::is_queue_policy_set () const
{
    return queue_policy != QMANAGER_OPTS_UNSET_STR;
}

bool queue_prop_t::is_queue_params_set () const
{
    return queue_params != QMANAGER_OPTS_UNSET_STR;
}

bool queue_prop_t::is_policy_params_set () const
{
    return policy_params != QMANAGER_OPTS_UNSET_STR;
}

qmanager_opts_t::qmanager_opts_t ()
{
    // Note: std::pair<>() is guaranteed to throw only an exception
    // from its arguments which in this case std::bad_alloc -- i.e.,
    // from std::string (const char *).
    // map<>::insert() can also throw std::bad_alloc.

    bool inserted = true;

    auto ret = m_tab.insert (std::pair<std::string, int> (
                   "queue-policy",
                   static_cast<int> (qmanager_opts_key_t::QUEUE_POLICY)));
    inserted &= ret.second;
    ret = m_tab.insert (std::pair<std::string, int> (
              "queue-params",
              static_cast<int> (qmanager_opts_key_t::QUEUE_PARAMS)));
    inserted &= ret.second;
    ret= m_tab.insert (std::pair<std::string, int> (
              "policy-params",
              static_cast<int> (qmanager_opts_key_t::POLICY_PARAMS)));
    inserted &= ret.second;

    if (!inserted)
        throw std::bad_alloc ();
}

void qmanager_opts_t::set_queue_policy (const std::string &o)
{
    m_queue_prop.queue_policy = o;
}

void qmanager_opts_t::set_queue_params (const std::string &o)
{
    m_queue_prop.queue_params = o;
}

void qmanager_opts_t::set_policy_params (const std::string &o)
{
    m_queue_prop.policy_params = o;
}

const std::string &qmanager_opts_t::get_queue_policy () const
{
    return m_queue_prop.queue_policy;
}

const std::string &qmanager_opts_t::get_queue_params () const
{
    return m_queue_prop.queue_params;
}

const std::string &qmanager_opts_t::get_policy_params () const
{
    return m_queue_prop.policy_params;
}

bool qmanager_opts_t::is_queue_policy_set () const
{
    return m_queue_prop.queue_policy != QMANAGER_OPTS_UNSET_STR;
}

bool qmanager_opts_t::is_queue_params_set () const
{
    return m_queue_prop.queue_params != QMANAGER_OPTS_UNSET_STR;
}

bool qmanager_opts_t::is_policy_params_set () const
{
    return m_queue_prop.policy_params != QMANAGER_OPTS_UNSET_STR;
}

qmanager_opts_t &qmanager_opts_t::operator+= (const qmanager_opts_t &src)
{
    if (src.m_queue_prop.queue_policy != QMANAGER_OPTS_UNSET_STR)
        m_queue_prop.queue_policy = src.m_queue_prop.queue_policy;
    if (src.m_queue_prop.queue_params != QMANAGER_OPTS_UNSET_STR)
        m_queue_prop.queue_params = src.m_queue_prop.queue_params;
    if (src.m_queue_prop.policy_params != QMANAGER_OPTS_UNSET_STR)
        m_queue_prop.policy_params = src.m_queue_prop.policy_params;
    return canonicalize ();
}

bool qmanager_opts_t::operator ()(const std::string &k1,
                                  const std::string &k2) const
{
    if (m_tab.find (k1) == m_tab.end ()
        || m_tab.find (k2) == m_tab.end ())
        return k1 < k2;
    return m_tab.at (k1) < m_tab.at (k2);
}

int qmanager_opts_t::parse (const std::string &k, const std::string &v,
                            std::string &info)
{
    int rc = 0;
    std::string dflt;
    int key = static_cast<int> (qmanager_opts_key_t::UNKNOWN);

    if (m_tab.find (k) != m_tab.end ())
        key = m_tab[k];

    switch (key) {
    case static_cast<int> (qmanager_opts_key_t::QUEUE_POLICY):
        dflt = m_queue_prop.queue_policy;
        m_queue_prop.queue_policy = v;
        if (!known_queue_policy (m_queue_prop.queue_policy)) {
            info += "Unknown queuing policy (" + v + ")! ";
            info += "Use default.";
            m_queue_prop.queue_policy = dflt;
        }
        break;

    case static_cast<int> (qmanager_opts_key_t::QUEUE_PARAMS):
        m_queue_prop.queue_params = v;
        break;

    case static_cast<int> (qmanager_opts_key_t::POLICY_PARAMS):
        m_queue_prop.policy_params = v;
        break;

    default:
        info += "Unknown option (" + k + ").";
        errno = EINVAL;
        rc = -1;
        break;
    }

    return rc;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
