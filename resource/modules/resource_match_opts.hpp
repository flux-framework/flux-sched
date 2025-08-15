/*****************************************************************************\
 * Copyright 2022 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef RESOURCE_MATCH_OPTS_HPP
#define RESOURCE_MATCH_OPTS_HPP

extern "C" {
#include <jansson.h>
}

#include <map>
#include <string>
#include "src/common/liboptmgr/optmgr.hpp"

namespace Flux {
namespace opts_manager {

const std::string RESOURCE_OPTS_UNSET_STR = "0xdeadbeef";

class resource_prop_t {
   public:
    const std::string &get_load_file () const;
    const std::string &get_load_format () const;
    const std::string &get_load_allowlist () const;
    const std::string &get_match_policy () const;
    const std::string &get_match_format () const;
    const std::string &get_match_subsystems () const;
    const int get_reserve_vtx_vec () const;
    const std::string &get_prune_filters () const;
    const resource_prop_t &get_resource_prop () const;
    const int get_update_interval () const;
    const std::string &get_traverser_policy () const;

    void set_load_file (const std::string &o);
    bool set_load_format (const std::string &o);
    void set_load_allowlist (const std::string &o);
    bool set_match_policy (const std::string &o, std::string &e);
    bool set_match_format (const std::string &o);
    void set_match_subsystems (const std::string &o);
    void set_reserve_vtx_vec (const int i);
    void set_prune_filters (const std::string &o);
    void add_to_prune_filters (const std::string &o);
    void set_update_interval (const int i);
    bool set_traverser_policy (const std::string &o);

    bool is_load_file_set () const;
    bool is_load_format_set () const;
    bool is_load_allowlist_set () const;
    bool is_match_policy_set () const;
    bool is_match_format_set () const;
    bool is_match_subsystems_set () const;
    bool is_reserve_vtx_vec_set () const;
    bool is_prune_filters_set () const;
    bool is_update_interval_set () const;
    bool is_traverser_policy_set () const;

    json_t *jsonify () const;

   private:
    std::string m_load_file = RESOURCE_OPTS_UNSET_STR;
    std::string m_load_format = RESOURCE_OPTS_UNSET_STR;
    std::string m_load_allowlist = RESOURCE_OPTS_UNSET_STR;
    std::string m_match_policy = RESOURCE_OPTS_UNSET_STR;
    std::string m_match_format = RESOURCE_OPTS_UNSET_STR;
    std::string m_match_subsystems = RESOURCE_OPTS_UNSET_STR;
    int m_reserve_vtx_vec = 0;
    std::string m_prune_filters = RESOURCE_OPTS_UNSET_STR;
    int m_update_interval = 0;
    std::string m_traverser_policy = RESOURCE_OPTS_UNSET_STR;
};

/*! resource match option set class
 */
class resource_opts_t : public optmgr_parse_t {
   public:
    enum class resource_opts_key_t : int {
        LOAD_FILE = 1,
        LOAD_FORMAT = 10,
        LOAD_ALLOWLIST = 20,
        MATCH_POLICY = 30,      // policy
        MATCH_FORMAT = 40,      // match-format
        MATCH_SUBSYSTEMS = 50,  // subsystem
        RESERVE_VTX_VEC = 60,   // reserve-vtx-vec
        PRUNE_FILTERS = 70,     // prune-filter
        UPDATE_INTERVAL = 80,   // update-interval
        TRAVERSER_POLICY = 90,  // traverser
        UNKNOWN = 5000
    };

    // These constructors can throw std::bad_alloc exception
    resource_opts_t ();
    resource_opts_t (const resource_opts_t &o) = default;
    resource_opts_t &operator= (const resource_opts_t &o) = default;
    // No exception is thrown from move constructor and move assignment
    resource_opts_t (resource_opts_t &&o) = default;
    resource_opts_t &operator= (resource_opts_t &&o) = default;

    const std::string &get_load_file () const;
    const std::string &get_load_format () const;
    const std::string &get_load_allowlist () const;
    const std::string &get_match_policy () const;
    const std::string &get_match_format () const;
    const std::string &get_match_subsystems () const;
    const int get_reserve_vtx_vec () const;
    const std::string &get_prune_filters () const;
    const resource_prop_t &get_resource_prop () const;
    const int get_update_interval () const;
    const std::string &get_traverser_policy () const;

    void set_load_file (const std::string &o);
    bool set_load_format (const std::string &o);
    void set_load_allowlist (const std::string &o);
    bool set_match_policy (const std::string &o, std::string &e);
    bool set_match_format (const std::string &o);
    void set_match_subsystems (const std::string &o);
    void set_reserve_vtx_vec (const int i);
    void set_prune_filters (const std::string &o);
    void set_update_interval (const int i);
    bool set_traverser_policy (const std::string &o);

    bool is_load_file_set () const;
    bool is_load_format_set () const;
    bool is_load_allowlist_set () const;
    bool is_match_policy_set () const;
    bool is_match_format_set () const;
    bool is_match_subsystems_set () const;
    bool is_reserve_vtx_vec_set () const;
    bool is_prune_filters_set () const;
    bool is_update_interval_set () const;
    bool is_traverser_policy_set () const;

    /*! Canonicalize the option set -- apply the general resource properties
     */
    resource_opts_t &canonicalize ();

    /*! Add an option with a higher precedence than "this" object.
     *  Modify this object according to the following "add" semantics
     *  and return the modified object:
     *  For each option key in o which has a non-default value,
     *  override the same key in this object.
     *
     *  \param o         an option set object with a higher precedence.
     *  \return          the composed resource_opts_t object.
     */
    resource_opts_t &operator+= (const resource_opts_t &o);

    /*! Comparator function to make this whole class be used
     *  as a functor -- allowing an upper class to call parse()
     *  with a specific iteration order among key-value pairs.
     *  \param k1        reference key
     *  \param k2        comparing key
     *  \return          true if k1 should precede k2 during iteration.
     */
    bool operator() (const std::string &k1, const std::string &k2) const;

    /*! Parse the value string (v) according to the key string (k).
     *  The parsed results are stored in "this" object.
     *
     *  \param k         key string
     *  \param v         value string
     *  \param info      parsing info and warning string to return.
     *  \return          0 on success; -1 on error.
     */
    int parse (const std::string &k, const std::string &v, std::string &info);

    /*! Return the set option parameters as an JSON string
     *
     *  \param json_out  output JSON string
     *  \return          0 on success; -1 on error.
     */
    int jsonify (std::string &json_out) const;

   private:
    bool is_number (const std::string &num_str);

    // default queue properties
    resource_prop_t m_resource_prop;

    // mapping each option to an integer
    std::map<std::string, int> m_tab;
};

}  // namespace opts_manager
}  // namespace Flux

#endif  // RESOURCE_MATCH_OPTS_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
