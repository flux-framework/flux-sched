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

using enum Flux::opts_manager::resource_opts_t::opt_key_t;

////////////////////////////////////////////////////////////////////////////////
// Private API for Resource Match Option Class
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

const std::map<std::string, resource_opts_t::opt_key_t> resource_opts_t::opt_key_map = {
    {"load-file", LOAD_FILE},
    {"load-format", LOAD_FORMAT},
    {"load-allowlist", LOAD_ALLOWLIST},
    {"policy", MATCH_POLICY},
    {"match-policy", MATCH_POLICY},
    {"match-format", MATCH_FORMAT},
    {"subsystems", MATCH_SUBSYSTEMS},
    {"reserve-vtx-vec", RESERVE_VTX_VEC},
    {"prune-filters", PRUNE_FILTERS},
    {"update-interval", UPDATE_INTERVAL},
    {"traverser", TRAVERSER_POLICY},
};

////////////////////////////////////////////////////////////////////////////////
// Public API for Resource Match Option Class
////////////////////////////////////////////////////////////////////////////////

const std::optional<std::string> &resource_opts_t::get_load_file () const
{
    return m_load_file;
}

const std::optional<std::string> &resource_opts_t::get_load_format () const
{
    return m_load_format;
}

const std::optional<std::string> &resource_opts_t::get_load_allowlist () const
{
    return m_load_allowlist;
}

const std::optional<std::string> &resource_opts_t::get_match_policy () const
{
    return m_match_policy;
}

const std::optional<std::string> &resource_opts_t::get_match_format () const
{
    return m_match_format;
}

const std::optional<std::string> &resource_opts_t::get_match_subsystems () const
{
    return m_match_subsystems;
}

const std::optional<int> resource_opts_t::get_reserve_vtx_vec () const
{
    return m_reserve_vtx_vec;
}

const std::optional<std::string> &resource_opts_t::get_prune_filters () const
{
    return m_prune_filters;
}

const std::optional<int> resource_opts_t::get_update_interval () const
{
    return m_update_interval;
}

const std::optional<std::string> &resource_opts_t::get_traverser_policy () const
{
    return m_traverser_policy;
}

void resource_opts_t::set_load_file (const std::string &p)
{
    m_load_file = p;
}

bool resource_opts_t::set_load_format (const std::string &p)
{
    if (!known_resource_reader (p))
        return false;
    m_load_format = p;
    return true;
}

void resource_opts_t::set_load_allowlist (const std::string &p)
{
    m_load_allowlist = p;
}

bool resource_opts_t::set_match_policy (const std::string &p, std::string &error_str)
{
    if (!known_match_policy (p, error_str))
        return false;
    m_match_policy = p;
    return true;
}

bool resource_opts_t::set_match_format (const std::string &p)
{
    if (!known_match_format (p))
        return false;
    m_match_format = p;
    return true;
}

void resource_opts_t::set_match_subsystems (const std::string &p)
{
    m_match_subsystems = p;
}

void resource_opts_t::set_reserve_vtx_vec (const int i)
{
    m_reserve_vtx_vec = i;
}

void resource_opts_t::set_prune_filters (const std::string &p)
{
    m_prune_filters = p;
}

bool resource_opts_t::set_traverser_policy (const std::string &p)
{
    if (!known_traverser_policy (p))
        return false;
    m_traverser_policy = p;
    return true;
}

void resource_opts_t::add_to_prune_filters (const std::string &p)
{
    m_prune_filters.transform ([&p] (std::string m_pf) { return m_pf + "," + p; });
}

void resource_opts_t::set_update_interval (const int i)
{
    m_update_interval = i;
}

resource_opts_t &resource_opts_t::canonicalize ()
{
    return *this;
}

resource_opts_t &resource_opts_t::operator+= (const resource_opts_t &src)
{
    if (auto src_load_file = src.get_load_file ())
        set_load_file (*src_load_file);
    if (auto src_load_format = src.get_load_format ())
        set_load_format (*src_load_format);
    if (auto src_load_allowlist = src.get_load_allowlist ())
        set_load_allowlist (*src_load_allowlist);
    if (auto src_match_policy = src.get_match_policy ()) {
        std::string e = "";
        set_match_policy (*src_match_policy, e);
    }
    if (auto src_match_format = src.get_match_format ())
        set_match_format (*src_match_format);
    if (auto src_match_subsystems = src.get_match_subsystems ())
        set_match_subsystems (*src_match_subsystems);
    if (auto src_reserve_vtx_vec = src.get_reserve_vtx_vec ())
        set_reserve_vtx_vec (*src_reserve_vtx_vec);
    if (auto src_prune_filters = src.get_prune_filters ())
        set_prune_filters (*src_prune_filters);
    if (auto src_update_interval = src.get_update_interval ())
        set_update_interval (*src_update_interval);
    if (auto src_traverser_policy = src.get_traverser_policy ())
        set_traverser_policy (*src_traverser_policy);
    return *this;
}

bool resource_opts_t::operator() (const std::string &k1, const std::string &k2) const
{
    if (opt_key_map.find (k1) == opt_key_map.end () || opt_key_map.find (k2) == opt_key_map.end ())
        return k1 < k2;
    return opt_key_map.at (k1) < opt_key_map.at (k2);
}

int resource_opts_t::parse (const std::string &k, const std::string &v, std::string &info)
{
    int rc = 0;
    resource_opts_t::opt_key_t opt_key = UNKNOWN;

    if (opt_key_map.find (k) != opt_key_map.end ())
        opt_key = opt_key_map.at (k);

    switch (opt_key) {
        case LOAD_FILE:
            set_load_file (v);
            break;

        case LOAD_FORMAT:
            if (!set_load_format (v)) {
                info += "Unknown resource reader (" + v + ")! ";
                info += "Using default.";
            }
            break;

        case LOAD_ALLOWLIST:
            set_load_allowlist (v);
            break;

        case MATCH_POLICY:
            if (!set_match_policy (v, info)) {
                info += "Unknown match policy (" + v + ")! ";
                errno = EINVAL;
                rc = -1;
                return rc;
            }
            break;

        case MATCH_FORMAT:
            if (!set_match_format (v)) {
                info += "Unknown match format (" + v + ")! ";
                info += "Using default.";
            }
            break;

        case MATCH_SUBSYSTEMS:
            set_match_subsystems (v);
            break;

        case RESERVE_VTX_VEC:
            if (is_number (v)) {
                int s = std::stoi (v);
                if (!(s <= 0 || s > 2000000)) {
                    set_reserve_vtx_vec (s);
                }
            }
            break;

        case PRUNE_FILTERS:
            if (v.find_first_not_of (' ') != std::string::npos) {
                if (get_prune_filters ())
                    add_to_prune_filters (v);
                else
                    set_prune_filters (v);
            }
            break;

        case UPDATE_INTERVAL:
            if (is_number (v)) {
                int s = std::stoi (v);
                if (!(s <= 0 || s > 2000000)) {
                    set_update_interval (s);
                }
            }
            break;

        case TRAVERSER_POLICY:
            if (!set_traverser_policy (v)) {
                info += "Unknown traverser policy (" + v + ")! ";
                errno = EINVAL;
                rc = -1;
                return rc;
            }
            break;

        case UNKNOWN:
            info += "Unknown option (" + k + ").";
            errno = EINVAL;
            rc = -1;
            break;
    }

    return rc;
}

int resource_opts_t::jsonify (std::string &json_out) const
{
    int rc = -1;
    int save_errno;
    json_t *o{nullptr};
    const char *json_str{nullptr};
    auto to_c_str = [] (auto &s) { return s.c_str (); };

    o = json_pack ("{ s:s? s:s? s:s? s:s? s:s? s:s? s:i s:s? s:i s:s? }",
                   "load-file",
                   get_load_file ().transform (to_c_str).value_or (nullptr),
                   "load-format",
                   get_load_format ().transform (to_c_str).value_or (nullptr),
                   "load-allowlist",
                   get_load_allowlist ().transform (to_c_str).value_or (nullptr),
                   "policy",
                   get_match_policy ().transform (to_c_str).value_or (nullptr),
                   "match-format",
                   get_match_format ().transform (to_c_str).value_or (nullptr),
                   "subsystems",
                   get_match_subsystems ().transform (to_c_str).value_or (nullptr),
                   "reserve-vtx-vec",
                   get_reserve_vtx_vec ().value_or (0),
                   "prune-filters",
                   get_prune_filters ().transform (to_c_str).value_or (nullptr),
                   "update-interval",
                   get_update_interval ().value_or (0),
                   "traverser",
                   get_traverser_policy ().transform (to_c_str).value_or (nullptr));
    if (!o) {
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

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
