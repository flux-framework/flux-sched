/*****************************************************************************\
 * Copyright 2022 Lawrence Livermore National Security, LLC
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

#include "resource_match_opts.hpp"
#include "resource/policies/dfu_match_policy_factory.hpp"
#include "resource/readers/resource_reader_factory.hpp"
#include "resource/writers/match_writers.hpp"
#include "resource/traversers/dfu_traverser_policy_factory.hpp"

using namespace Flux;
using namespace Flux::resource_model;
using namespace Flux::opts_manager;

////////////////////////////////////////////////////////////////////////////////
// Public API for Resource Match Option Class
////////////////////////////////////////////////////////////////////////////////

const std::string &resource_prop_t::get_load_file () const
{
    return m_load_file;
}

const std::string &resource_prop_t::get_load_format () const
{
    return m_load_format;
}

const std::string &resource_prop_t::get_load_allowlist () const
{
    return m_load_allowlist;
}

const std::string &resource_prop_t::get_match_policy () const
{
    return m_match_policy;
}

const std::string &resource_prop_t::get_match_format () const
{
    return m_match_format;
}

const std::string &resource_prop_t::get_match_subsystems () const
{
    return m_match_subsystems;
}

const int resource_prop_t::get_reserve_vtx_vec () const
{
    return m_reserve_vtx_vec;
}

const std::string &resource_prop_t::get_prune_filters () const
{
    return m_prune_filters;
}

const int resource_prop_t::get_update_interval () const
{
    return m_update_interval;
}

const std::string &resource_prop_t::get_traverser_policy () const
{
    return m_traverser_policy;
}

void resource_prop_t::set_load_file (const std::string &p)
{
    m_load_file = p;
}

bool resource_prop_t::set_load_format (const std::string &p)
{
    if (!known_resource_reader (p))
        return false;
    m_load_format = p;
    return true;
}

void resource_prop_t::set_load_allowlist (const std::string &p)
{
    m_load_allowlist = p;
}

bool resource_prop_t::set_match_policy (const std::string &p, std::string &error_str)
{
    if (!known_match_policy (p, error_str))
        return false;
    m_match_policy = p;
    return true;
}

bool resource_prop_t::set_match_format (const std::string &p)
{
    if (!known_match_format (p))
        return false;
    m_match_format = p;
    return true;
}

void resource_prop_t::set_match_subsystems (const std::string &p)
{
    m_match_subsystems = p;
}

void resource_prop_t::set_reserve_vtx_vec (const int i)
{
    m_reserve_vtx_vec = i;
}

void resource_prop_t::set_prune_filters (const std::string &p)
{
    m_prune_filters = p;
}

bool resource_prop_t::set_traverser_policy (const std::string &p)
{
    if (!known_traverser_policy (p))
        return false;
    m_traverser_policy = p;
    return true;
}

void resource_prop_t::add_to_prune_filters (const std::string &p)
{
    m_prune_filters += ",";
    m_prune_filters += p;
}

void resource_prop_t::set_update_interval (const int i)
{
    m_update_interval = i;
}

bool resource_prop_t::is_load_file_set () const
{
    return m_load_file != RESOURCE_OPTS_UNSET_STR;
}

bool resource_prop_t::is_load_format_set () const
{
    return m_load_format != RESOURCE_OPTS_UNSET_STR;
}

bool resource_prop_t::is_load_allowlist_set () const
{
    return m_load_allowlist != RESOURCE_OPTS_UNSET_STR;
}

bool resource_prop_t::is_match_policy_set () const
{
    return m_match_policy != RESOURCE_OPTS_UNSET_STR;
}

bool resource_prop_t::is_match_format_set () const
{
    return m_match_format != RESOURCE_OPTS_UNSET_STR;
}

bool resource_prop_t::is_match_subsystems_set () const
{
    return m_match_subsystems != RESOURCE_OPTS_UNSET_STR;
}

bool resource_prop_t::is_reserve_vtx_vec_set () const
{
    return m_reserve_vtx_vec != 0;
}

bool resource_prop_t::is_prune_filters_set () const
{
    return m_prune_filters != RESOURCE_OPTS_UNSET_STR;
}

bool resource_prop_t::is_update_interval_set () const
{
    return m_update_interval != 0;
}

bool resource_prop_t::is_traverser_policy_set () const
{
    return m_traverser_policy != RESOURCE_OPTS_UNSET_STR;
}

json_t *resource_prop_t::jsonify () const
{
    return json_pack ("{ s:s? s:s? s:s? s:s? s:s? s:s? s:i s:s? s:i s:s? }",
                      "load-file",
                      is_load_file_set () ? get_load_file ().c_str () : nullptr,
                      "load-format",
                      is_load_format_set () ? get_load_format ().c_str () : nullptr,
                      "load-allowlist",
                      is_load_allowlist_set () ? get_load_allowlist ().c_str () : nullptr,
                      "policy",
                      is_match_policy_set () ? get_match_policy ().c_str () : nullptr,
                      "match-format",
                      is_match_format_set () ? get_match_format ().c_str () : nullptr,
                      "subsystems",
                      is_match_subsystems_set () ? get_match_subsystems ().c_str () : nullptr,
                      "reserve-vtx-vec",
                      is_reserve_vtx_vec_set () ? get_reserve_vtx_vec () : 0,
                      "prune-filters",
                      is_prune_filters_set () ? get_prune_filters ().c_str () : nullptr,
                      "update-interval",
                      is_update_interval_set () ? get_update_interval () : 0,
                      "traverser",
                      is_traverser_policy_set () ? get_traverser_policy ().c_str () : nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// Private API for Resource Match Property Class
////////////////////////////////////////////////////////////////////////////////

bool resource_opts_t::is_number (const std::string &num_str)
{
    if (num_str.empty ())
        return false;
    auto i = std::find_if (num_str.begin (), num_str.end (), [] (unsigned char c) {
        return !std::isdigit (c);
    });
    return i == num_str.end ();
}

////////////////////////////////////////////////////////////////////////////////
// Public API for Resource Match Property Class
////////////////////////////////////////////////////////////////////////////////

resource_opts_t::resource_opts_t ()
{
    // Note: std::pair<>() is guaranteed to throw only an exception
    // from its arguments which in this case std::bad_alloc -- i.e.,
    // from std::string (const char *).
    // map<>::insert() can also throw std::bad_alloc.

    bool inserted = true;

    auto ret = m_tab.insert (
        std::pair<std::string, int> ("load-file",
                                     static_cast<int> (
                                         resource_opts_t ::resource_opts_key_t ::LOAD_FILE)));
    inserted &= ret.second;
    ret = m_tab.insert (
        std::pair<std::string, int> ("load-format",
                                     static_cast<int> (
                                         resource_opts_t ::resource_opts_key_t ::LOAD_FORMAT)));
    inserted &= ret.second;
    ret = m_tab.insert (
        std::pair<std::string, int> ("load-allowlist",
                                     static_cast<int> (
                                         resource_opts_t ::resource_opts_key_t ::LOAD_ALLOWLIST)));
    inserted &= ret.second;
    ret = m_tab.insert (
        std::pair<std::string, int> ("policy",
                                     static_cast<int> (
                                         resource_opts_t ::resource_opts_key_t ::MATCH_POLICY)));
    inserted &= ret.second;
    ret = m_tab.insert (
        std::pair<std::string, int> ("match-policy",
                                     static_cast<int> (
                                         resource_opts_t ::resource_opts_key_t ::MATCH_POLICY)));
    inserted &= ret.second;
    ret = m_tab.insert (
        std::pair<std::string, int> ("match-format",
                                     static_cast<int> (
                                         resource_opts_t ::resource_opts_key_t ::MATCH_FORMAT)));
    inserted &= ret.second;
    ret = m_tab.insert (
        std::pair<std::string,
                  int> ("subsystems",
                        static_cast<int> (
                            resource_opts_t ::resource_opts_key_t ::MATCH_SUBSYSTEMS)));
    inserted &= ret.second;
    ret = m_tab.insert (
        std::pair<std::string, int> ("reserve-vtx-vec",
                                     static_cast<int> (
                                         resource_opts_t ::resource_opts_key_t ::RESERVE_VTX_VEC)));
    inserted &= ret.second;
    ret = m_tab.insert (
        std::pair<std::string, int> ("prune-filters",
                                     static_cast<int> (
                                         resource_opts_t ::resource_opts_key_t ::PRUNE_FILTERS)));
    inserted &= ret.second;
    ret = m_tab.insert (
        std::pair<std::string, int> ("update-interval",
                                     static_cast<int> (
                                         resource_opts_t ::resource_opts_key_t ::UPDATE_INTERVAL)));
    inserted &= ret.second;
    ret = m_tab.insert (
        std::pair<std::string,
                  int> ("traverser",
                        static_cast<int> (
                            resource_opts_t ::resource_opts_key_t ::TRAVERSER_POLICY)));
    inserted &= ret.second;

    if (!inserted)
        throw std::bad_alloc ();
}

const std::string &resource_opts_t::get_load_file () const
{
    return m_resource_prop.get_load_file ();
}

const std::string &resource_opts_t::get_load_format () const
{
    return m_resource_prop.get_load_format ();
}

const std::string &resource_opts_t::get_load_allowlist () const
{
    return m_resource_prop.get_load_allowlist ();
}

const std::string &resource_opts_t::get_match_policy () const
{
    return m_resource_prop.get_match_policy ();
}

const std::string &resource_opts_t::get_match_format () const
{
    return m_resource_prop.get_match_format ();
}

const std::string &resource_opts_t::get_match_subsystems () const
{
    return m_resource_prop.get_match_subsystems ();
}

const int resource_opts_t::get_reserve_vtx_vec () const
{
    return m_resource_prop.get_reserve_vtx_vec ();
}

const std::string &resource_opts_t::get_prune_filters () const
{
    return m_resource_prop.get_prune_filters ();
}

const int resource_opts_t::get_update_interval () const
{
    return m_resource_prop.get_update_interval ();
}

const resource_prop_t &resource_opts_t::get_resource_prop () const
{
    return m_resource_prop;
}

const std::string &resource_opts_t::get_traverser_policy () const
{
    return m_resource_prop.get_traverser_policy ();
}

void resource_opts_t::set_load_file (const std::string &o)
{
    m_resource_prop.set_load_file (o);
}

bool resource_opts_t::set_load_format (const std::string &o)
{
    return m_resource_prop.set_load_format (o);
}

void resource_opts_t::set_load_allowlist (const std::string &o)
{
    m_resource_prop.set_load_allowlist (o);
}

bool resource_opts_t::set_match_policy (const std::string &o, std::string &e)
{
    return m_resource_prop.set_match_policy (o, e);
}

bool resource_opts_t::set_match_format (const std::string &o)
{
    return m_resource_prop.set_match_format (o);
}

void resource_opts_t::set_match_subsystems (const std::string &o)
{
    m_resource_prop.set_match_subsystems (o);
}

void resource_opts_t::set_reserve_vtx_vec (const int i)
{
    m_resource_prop.set_reserve_vtx_vec (i);
}

void resource_opts_t::set_prune_filters (const std::string &o)
{
    m_resource_prop.set_prune_filters (o);
}

void resource_opts_t::set_update_interval (const int i)
{
    m_resource_prop.set_update_interval (i);
}

bool resource_opts_t::set_traverser_policy (const std::string &o)
{
    return m_resource_prop.set_traverser_policy (o);
}

bool resource_opts_t::is_load_file_set () const
{
    return m_resource_prop.is_load_file_set ();
}

bool resource_opts_t::is_load_format_set () const
{
    return m_resource_prop.is_load_format_set ();
}

bool resource_opts_t::is_load_allowlist_set () const
{
    return m_resource_prop.is_load_allowlist_set ();
}

bool resource_opts_t::is_match_policy_set () const
{
    return m_resource_prop.is_match_policy_set ();
}

bool resource_opts_t::is_match_format_set () const
{
    return m_resource_prop.is_match_format_set ();
}

bool resource_opts_t::is_match_subsystems_set () const
{
    return m_resource_prop.is_match_subsystems_set ();
}

bool resource_opts_t::is_reserve_vtx_vec_set () const
{
    return m_resource_prop.is_reserve_vtx_vec_set ();
}

bool resource_opts_t::is_prune_filters_set () const
{
    return m_resource_prop.is_prune_filters_set ();
}

bool resource_opts_t::is_update_interval_set () const
{
    return m_resource_prop.is_update_interval_set ();
}

bool resource_opts_t::is_traverser_policy_set () const
{
    return m_resource_prop.is_traverser_policy_set ();
}

resource_opts_t &resource_opts_t::canonicalize ()
{
    return *this;
}

resource_opts_t &resource_opts_t::operator+= (const resource_opts_t &src)
{
    if (src.m_resource_prop.is_load_file_set ())
        m_resource_prop.set_load_file (src.m_resource_prop.get_load_file ());
    if (src.m_resource_prop.is_load_format_set ())
        m_resource_prop.set_load_format (src.m_resource_prop.get_load_format ());
    if (src.m_resource_prop.is_load_allowlist_set ())
        m_resource_prop.set_load_allowlist (src.m_resource_prop.get_load_allowlist ());
    if (src.m_resource_prop.is_match_policy_set ()) {
        std::string e = "";
        m_resource_prop.set_match_policy (src.m_resource_prop.get_match_policy (), e);
    }
    if (src.m_resource_prop.is_match_format_set ())
        m_resource_prop.set_match_format (src.m_resource_prop.get_match_format ());
    if (src.m_resource_prop.is_match_subsystems_set ())
        m_resource_prop.set_match_subsystems (src.m_resource_prop.get_match_subsystems ());
    if (src.m_resource_prop.is_reserve_vtx_vec_set ())
        m_resource_prop.set_reserve_vtx_vec (src.m_resource_prop.get_reserve_vtx_vec ());
    if (src.m_resource_prop.is_prune_filters_set ())
        m_resource_prop.set_prune_filters (src.m_resource_prop.get_prune_filters ());
    if (src.m_resource_prop.is_update_interval_set ())
        m_resource_prop.set_update_interval (src.m_resource_prop.get_update_interval ());
    if (src.m_resource_prop.is_traverser_policy_set ())
        m_resource_prop.set_traverser_policy (src.m_resource_prop.get_traverser_policy ());
    return *this;
}

bool resource_opts_t::operator() (const std::string &k1, const std::string &k2) const
{
    if (m_tab.find (k1) == m_tab.end () || m_tab.find (k2) == m_tab.end ())
        return k1 < k2;
    return m_tab.at (k1) < m_tab.at (k2);
}

int resource_opts_t::jsonify (std::string &json_out) const
{
    int rc = -1;
    int save_errno;
    json_t *o{nullptr};
    const char *json_str{nullptr};

    if (!(o = m_resource_prop.jsonify ())) {
        errno = ENOMEM;
        goto ret;
    }
    if (!(json_str = json_dumps (o, JSON_INDENT (0)))) {
        errno = ENOMEM;
        goto ret;
    }
    json_out = json_str;
    rc = 0;

ret:
    save_errno = errno;
    if (o)
        json_decref (o);
    free ((char *)json_str);
    errno = save_errno;
    return rc;
}

int resource_opts_t::parse (const std::string &k, const std::string &v, std::string &info)
{
    int rc = 0;
    std::string dflt;
    std::map<std::string, std::string> tmp_mp;
    int key = static_cast<int> (resource_opts_key_t::UNKNOWN);

    if (m_tab.find (k) != m_tab.end ())
        key = m_tab[k];

    switch (key) {
        case static_cast<int> (resource_opts_key_t::LOAD_FILE):
            m_resource_prop.set_load_file (v);
            break;

        case static_cast<int> (resource_opts_key_t::LOAD_FORMAT):
            if (!m_resource_prop.set_load_format (v)) {
                info += "Unknown resource reader (" + v + ")! ";
                info += "Using default.";
            }
            break;

        case static_cast<int> (resource_opts_key_t::LOAD_ALLOWLIST):
            m_resource_prop.set_load_allowlist (v);
            break;

        case static_cast<int> (resource_opts_key_t::MATCH_POLICY):
            if (!m_resource_prop.set_match_policy (v, info)) {
                info += "Unknown match policy (" + v + ")! ";
                errno = EINVAL;
                rc = -1;
                return rc;
            }
            break;

        case static_cast<int> (resource_opts_key_t::MATCH_FORMAT):
            if (!m_resource_prop.set_match_format (v)) {
                info += "Unknown match format (" + v + ")! ";
                info += "Using default.";
            }
            break;

        case static_cast<int> (resource_opts_key_t::MATCH_SUBSYSTEMS):
            m_resource_prop.set_match_subsystems (v);
            break;

        case static_cast<int> (resource_opts_key_t::RESERVE_VTX_VEC):
            if (is_number (v)) {
                int s = std::stoi (v);
                if (!(s <= 0 || s > 2000000)) {
                    m_resource_prop.set_reserve_vtx_vec (s);
                }
            }
            break;

        case static_cast<int> (resource_opts_key_t::PRUNE_FILTERS):
            if (v.find_first_not_of (' ') != std::string::npos) {
                if (m_resource_prop.is_prune_filters_set ())
                    m_resource_prop.add_to_prune_filters (v);
                else
                    m_resource_prop.set_prune_filters (v);
            }
            break;

        case static_cast<int> (resource_opts_key_t::UPDATE_INTERVAL):
            if (is_number (v)) {
                int s = std::stoi (v);
                if (!(s <= 0 || s > 2000000)) {
                    m_resource_prop.set_update_interval (s);
                }
            }
            break;

        case static_cast<int> (resource_opts_key_t::TRAVERSER_POLICY):
            if (!m_resource_prop.set_traverser_policy (v)) {
                info += "Unknown traverser policy (" + v + ")! ";
                errno = EINVAL;
                rc = -1;
                return rc;
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
