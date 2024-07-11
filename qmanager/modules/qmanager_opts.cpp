/*****************************************************************************\
 * Copyright 2020 Lawrence Livermore National Security, LLC
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

#include "qmanager_opts.hpp"

using namespace Flux;
using namespace Flux::opts_manager;

////////////////////////////////////////////////////////////////////////////////
// Private API for Queue Manager Option Class
////////////////////////////////////////////////////////////////////////////////

int qmanager_opts_t::parse_queues (const std::string &queues)
{
    int rc = 0;
    try {
        std::vector<std::string> entries;
        if ((rc = parse_multi (queues.c_str (), ' ', entries)) < 0)
            goto done;
        m_per_queue_prop.clear ();  // clear the default queue entry
        for (const auto &entry : entries) {
            auto ret = m_per_queue_prop.insert (
                std::pair<std::string, queue_prop_t> (entry, queue_prop_t ()));
            if (!ret.second) {
                errno = EEXIST;
                rc = -1;
                goto done;
            }
        }
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        rc = -1;
    }
done:
    return rc;
}

////////////////////////////////////////////////////////////////////////////////
// Private API for Queue Property Class
////////////////////////////////////////////////////////////////////////////////

bool queue_prop_t::known_queue_policy (const std::string &policy)
{
    bool rc = false;
    if (policy == "fcfs" || policy == "easy" || policy == "hybrid" || policy == "conservative")
        rc = true;
    return rc;
}

////////////////////////////////////////////////////////////////////////////////
// Public API for Queue Manager Option Class
////////////////////////////////////////////////////////////////////////////////

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

json_t *queue_prop_t::jsonify () const
{
    return json_pack ("{ s:s? s:s? s:s? }",
                      "queue-policy",
                      is_queue_policy_set () ? queue_policy.c_str () : nullptr,
                      "queue-params",
                      is_queue_params_set () ? queue_params.c_str () : nullptr,
                      "policy-params",
                      is_policy_params_set () ? policy_params.c_str () : nullptr);
}

qmanager_opts_t::qmanager_opts_t ()
{
    // Note: std::pair<>() is guaranteed to throw only an exception
    // from its arguments which in this case std::bad_alloc -- i.e.,
    // from std::string (const char *).
    // map<>::insert() can also throw std::bad_alloc.

    bool inserted = true;

    auto ret = m_tab.insert (
        std::pair<std::string, int> ("queues", static_cast<int> (qmanager_opts_key_t::QUEUES)));
    inserted &= ret.second;
    ret = m_tab.insert (
        std::pair<std::string, int> ("queue-policy",
                                     static_cast<int> (qmanager_opts_key_t::QUEUE_POLICY)));
    inserted &= ret.second;
    ret = m_tab.insert (
        std::pair<std::string, int> ("queue-params",
                                     static_cast<int> (qmanager_opts_key_t::QUEUE_PARAMS)));
    inserted &= ret.second;
    ret = m_tab.insert (
        std::pair<std::string, int> ("policy-params",
                                     static_cast<int> (qmanager_opts_key_t::POLICY_PARAMS)));
    inserted &= ret.second;
    ret = m_tab.insert (
        std::pair<std::string, int> ("queue-policy-per-queue",
                                     static_cast<int> (
                                         qmanager_opts_key_t::QUEUE_POLICY_PER_QUEUE)));
    inserted &= ret.second;
    ret = m_tab.insert (
        std::pair<std::string, int> ("queue-params-per-queue",
                                     static_cast<int> (
                                         qmanager_opts_key_t::QUEUE_PARAMS_PER_QUEUE)));
    inserted &= ret.second;
    ret = m_tab.insert (
        std::pair<std::string, int> ("policy-params-per-queue",
                                     static_cast<int> (
                                         qmanager_opts_key_t::POLICY_PARAMS_PER_QUEUE)));
    inserted &= ret.second;

    if (!inserted)
        throw std::bad_alloc ();
}

bool qmanager_opts_t::set_queue_policy (const std::string &o)
{
    return m_queue_prop.set_queue_policy (o);
}

void qmanager_opts_t::set_queue_params (const std::string &o)
{
    m_queue_prop.set_queue_params (o);
}

void qmanager_opts_t::set_policy_params (const std::string &o)
{
    m_queue_prop.set_policy_params (o);
}

const std::string &qmanager_opts_t::get_default_queue_name () const
{
    return m_default_queue_name;
}

const queue_prop_t &qmanager_opts_t::get_queue_prop () const
{
    return m_queue_prop;
}

const std::string &qmanager_opts_t::get_queue_policy () const
{
    return m_queue_prop.get_queue_policy ();
}

const std::string &qmanager_opts_t::get_queue_params () const
{
    return m_queue_prop.get_queue_params ();
}

const std::string &qmanager_opts_t::get_policy_params () const
{
    return m_queue_prop.get_policy_params ();
}

const std::map<std::string, queue_prop_t> &qmanager_opts_t::get_per_queue_prop () const
{
    return m_per_queue_prop;
}

bool qmanager_opts_t::is_queue_policy_set () const
{
    return m_queue_prop.is_queue_policy_set ();
}

bool qmanager_opts_t::is_queue_params_set () const
{
    return m_queue_prop.is_queue_params_set ();
}

bool qmanager_opts_t::is_policy_params_set () const
{
    return m_queue_prop.is_policy_params_set ();
}

qmanager_opts_t &qmanager_opts_t::canonicalize ()
{
    if (m_per_queue_prop.empty ()) {
        std::string qn = m_default_queue_name;
        auto ret =
            m_per_queue_prop.insert (std::pair<std::string, queue_prop_t> (qn, queue_prop_t ()));
        if (!ret.second)
            throw std::bad_alloc ();
    }
    for (auto &kv : m_per_queue_prop) {
        if (!kv.second.is_queue_policy_set ())
            kv.second.set_queue_policy (m_queue_prop.get_queue_policy ());
        if (!kv.second.is_queue_params_set ())
            kv.second.set_queue_params (m_queue_prop.get_queue_params ());
        if (!kv.second.is_policy_params_set ())
            kv.second.set_policy_params (m_queue_prop.get_policy_params ());
    }
    return *this;
}

qmanager_opts_t &qmanager_opts_t::operator+= (const qmanager_opts_t &src)
{
    if (src.m_queue_prop.is_queue_policy_set ())
        m_queue_prop.set_queue_policy (src.m_queue_prop.get_queue_policy ());
    if (src.m_queue_prop.is_queue_params_set ())
        m_queue_prop.set_queue_params (src.m_queue_prop.get_queue_params ());
    if (src.m_queue_prop.is_policy_params_set ())
        m_queue_prop.set_policy_params (src.m_queue_prop.get_policy_params ());
    if (!src.m_per_queue_prop.empty ())
        m_per_queue_prop = src.get_per_queue_prop ();
    return *this;
}

bool qmanager_opts_t::operator() (const std::string &k1, const std::string &k2) const
{
    if (m_tab.find (k1) == m_tab.end () || m_tab.find (k2) == m_tab.end ())
        return k1 < k2;
    return m_tab.at (k1) < m_tab.at (k2);
}

int qmanager_opts_t::jsonify (std::string &json_out) const
{
    int rc = -1;
    int save_errno;
    json_t *o{nullptr};
    const char *json_str{nullptr};

    if (!(o = m_queue_prop.jsonify ())) {
        errno = ENOMEM;
        goto ret;
    }
    for (auto &kv : m_per_queue_prop) {
        json_t *sub_o{nullptr};
        if (!(sub_o = kv.second.jsonify ())) {
            json_decref (sub_o);
            errno = ENOMEM;
            goto ret;
        }
        if (json_object_set_new (o, kv.first.c_str (), sub_o) < 0) {
            errno = ENOMEM;
            goto ret;
        }
    }
    if (!(json_str = json_dumps (o, JSON_INDENT (0)))) {
        errno = ENOMEM;
        goto ret;
    }
    json_out = json_str;
    rc = 0;

ret:
    save_errno = errno;
    json_decref (o);
    free ((char *)json_str);
    errno = save_errno;
    return rc;
}

int qmanager_opts_t::parse (const std::string &k, const std::string &v, std::string &info)
{
    int rc = 0;
    std::string dflt;
    std::map<std::string, std::string> tmp_mp;
    int key = static_cast<int> (qmanager_opts_key_t::UNKNOWN);

    if (m_tab.find (k) != m_tab.end ())
        key = m_tab[k];

    switch (key) {
        case static_cast<int> (qmanager_opts_key_t::QUEUES):
            rc = parse_queues (v);
            break;

        case static_cast<int> (qmanager_opts_key_t::QUEUE_POLICY):
            if (!m_queue_prop.set_queue_policy (v)) {
                info += "Unknown queuing policy (" + v + ")! ";
                info += "Using default.";
            }
            break;

        case static_cast<int> (qmanager_opts_key_t::QUEUE_PARAMS):
            m_queue_prop.set_queue_params (v);
            break;

        case static_cast<int> (qmanager_opts_key_t::POLICY_PARAMS):
            m_queue_prop.set_policy_params (v);
            break;

        case static_cast<int> (qmanager_opts_key_t::QUEUE_POLICY_PER_QUEUE):
            tmp_mp.clear ();
            if ((rc = parse_multi_options (v, ' ', ':', tmp_mp)) < 0)
                break;
            for (const auto &kv : tmp_mp) {
                if (m_per_queue_prop.find (kv.first) == m_per_queue_prop.end ()) {
                    info += "Unknown queue (" + kv.first + ").";
                    errno = ENOENT;
                    rc = -1;
                    break;
                }
                if (!m_per_queue_prop[kv.first].set_queue_policy (kv.second)) {
                    info += "Unknown queuing policy (" + v + ") for queue (" + kv.second + ")! ";
                    info += "Using default. ";
                }
            }
            break;

        case static_cast<int> (qmanager_opts_key_t::QUEUE_PARAMS_PER_QUEUE):
            tmp_mp.clear ();
            if ((rc = parse_multi_options (v, ' ', ':', tmp_mp)) < 0)
                break;
            for (const auto &kv : tmp_mp) {
                if (m_per_queue_prop.find (kv.first) == m_per_queue_prop.end ()) {
                    info += "Unknown queue (" + kv.first + ").";
                    errno = ENOENT;
                    rc = -1;
                    break;
                }
                m_per_queue_prop[kv.first].set_queue_params (kv.second);
            }
            break;

        case static_cast<int> (qmanager_opts_key_t::POLICY_PARAMS_PER_QUEUE):
            tmp_mp.clear ();
            if ((rc = parse_multi_options (v, ' ', ':', tmp_mp)) < 0)
                break;
            for (const auto &kv : tmp_mp) {
                if (m_per_queue_prop.find (kv.first) == m_per_queue_prop.end ()) {
                    info += "Unknown queue (" + kv.first + ").";
                    errno = ENOENT;
                    rc = -1;
                    break;
                }
                m_per_queue_prop[kv.first].set_policy_params (kv.second);
            }
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
