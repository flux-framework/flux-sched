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

#ifndef QMANAGER_OPTS_HPP
#define QMANAGER_OPTS_HPP

#include <string>
#include <vector>
#include <map>
#include "src/common/liboptmgr/optmgr.hpp"

namespace Flux {
namespace opts_manager {

const std::string QMANAGER_OPTS_UNSET_STR = "0xdeadbeef";

class queue_prop_t {
public:
    const std::string &get_queue_policy () const;
    const std::string &get_queue_params () const;
    const std::string &get_policy_params () const;

    bool set_queue_policy (const std::string &p);
    void set_queue_params (const std::string &p);
    void set_policy_params (const std::string &p);

    bool is_queue_policy_set () const;
    bool is_queue_params_set () const;
    bool is_policy_params_set () const;

private:
    bool known_queue_policy (const std::string &policy);

    std::string queue_policy = QMANAGER_OPTS_UNSET_STR;
    std::string queue_params = QMANAGER_OPTS_UNSET_STR;
    std::string policy_params = QMANAGER_OPTS_UNSET_STR;
};

/*! qmanager option set class
 *
 */
class qmanager_opts_t : public optmgr_parse_t {
public:
    enum class qmanager_opts_key_t : int {
        QUEUES                    = 0,  // queues
        DEFAULT_QUEUE             = 1,  // default-queue
        QUEUE_POLICY              = 10, // queue-policy
        QUEUE_PARAMS              = 20, // queue-params
        POLICY_PARAMS             = 30, // policy-params
        QUEUE_POLICY_PER_QUEUE    = 40, // queue-policy-per_queue
        QUEUE_PARAMS_PER_QUEUE    = 50, // queue-params-per_queue
        POLICY_PARAMS_PER_QUEUE   = 60, // policy-params-per_queue
        UNKNOWN                   = 5000
    };

    // These constructors can throw std::bad_alloc exception
    qmanager_opts_t ();
    qmanager_opts_t (const qmanager_opts_t &o) = default;
    qmanager_opts_t &operator= (const qmanager_opts_t &o) = default;
    // No exception is thrown from move constructor and move assignment
    qmanager_opts_t (qmanager_opts_t &&o) = default;
    qmanager_opts_t &operator= (qmanager_opts_t &&o) = default;

    void set_default_queue (const std::string &o);
    bool set_queue_policy (const std::string &o);
    void set_queue_params (const std::string &o);
    void set_policy_params (const std::string &o);

    const std::string &get_default_queue_name () const;
    const std::string &get_default_queue () const;
    const queue_prop_t &get_queue_prop () const;
    const std::string &get_queue_policy () const;
    const std::string &get_queue_params () const;
    const std::string &get_policy_params () const;
    const std::map<std::string, queue_prop_t> &get_per_queue_prop () const;

    bool is_default_queue_set () const;
    bool is_queue_policy_set () const;
    bool is_queue_params_set () const;
    bool is_policy_params_set () const;

    /*! Canonicalize the option set -- apply the general queue properities
     *  to per_queue queue properities if the latters are not explictly set.
     */
    qmanager_opts_t &canonicalize ();

    /*! Add an option with a higher precendence than "this" object.
     *  Modify this object according to the following "add" semantics
     *  and return the modified object:
     *  For each option key in o which has a non-default value,
     *  override the same key in this object.
     *
     *  \param o         an option set object with a higher precendence.
     *  \return          the composed qmanager_opts_t object.
     */
    qmanager_opts_t &operator+= (const qmanager_opts_t &o);

    /*! Comparator function to make this whole class be used
     *  as a functor -- allowing an upper class to call parse()
     *  with a specific iteration order among key-value pairs.
     *  \param k1        reference key
     *  \param k2        comparing key
     *  \return          true if k1 should precede k2 during iteration.
     */
    bool operator ()(const std::string &k1, const std::string &k2) const;

    /*! Parse the value string (v) according to the key string (k).
     *  The parsed results are stored in "this" object.
     *
     *  \param k         key string
     *  \param v         value string
     *  \param info      parsing info and warning string to return.
     *  \return          0 on success; -1 on error.
     */
    int parse (const std::string &k, const std::string &v, std::string &info);

private:
    int parse_queues (const std::string &queues);

    const std::string m_default_queue_name = "default";
    std::string m_default_queue = QMANAGER_OPTS_UNSET_STR;
    queue_prop_t m_queue_prop;
    std::map<std::string, queue_prop_t> m_per_queue_prop;
    std::map<std::string, int> m_tab;
};

} // namespace Flux
} // namespace opts_manager

#endif // QMANAGER_OPTS_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
