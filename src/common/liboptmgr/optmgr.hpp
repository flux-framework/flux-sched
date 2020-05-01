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
#ifndef OPTMGR_HPP
#define OPTMGR_HPP

#include <map>
#include <string>
#include <vector>
#include "src/common/liboptmgr/optmgr_impl.hpp"

namespace Flux {
namespace opts_manager {


/*! Flux module option composer class: Module options can come
 *  from multiple sources (e.g., through compile-time default,
 *  configuration file and module load options). This simple templated
 *  class helps compose the options from those multiple sources
 *  with specific precedences.
 *
 *  T is a Flux module option-set data type, representing an option
 *  set from a single source. optmgr_composer_t requires the T to
 *  provide the operator+=() method that implements how "this" T
 *  object must be composed with another T object with a higher precedence.
 *
 *  Expectation: operator+=() would newly add an option from the option
 *  set managed by the passed T object if "this" object does not already
 *  have it; otherwise override it. However, this wrapper class simply
 *  delegates the composition rule down to the template T class.
 */
template <class T>
class optmgr_composer_t {
public:

    /*!
     * Compose the option-set state managed by "this" object
     * with the option-set state from another object with
     * a higher precedence.
     * If T doesn't have operator+=(), a compilation error
     * will ensue.
     *
     * \param o        option-set object of T type 
     * \return         the option set object of type T of "this"
     *                 after composed with o.
     */
    T &operator+= (const T &o) {
        return m_opt += o;
    }

    /*!
     * Canonicalize the option set -- calls the same named method
     * of the object of the option set class T which canonicalizes
     * its internal state after all the composition completes.
     */
    T &canonicalize () {
        return m_opt.canonicalize ();
    }

    /*!
     * Getter
     *
     * \return         Option set object of type T
     */
    const T &get_opt () const {
        return m_opt;
    }

private:
    T m_opt;
};


/*! A key value store class that helps parsing and holding
 *  the option set from a single source (e.g., an option set passed from
 *  module-load time).
 *  Template class T should be a module option-set management class
 *  that provides a "parse" method. This method must take in an option
 *  key-value pair from the optmgr_kv_t class and update its state.
 */
template <class T>
struct optmgr_kv_t : public detail::optmgr_kv_impl_t<T> {

    /*! Getter
     *
     * \return         Option set object of type T of "this" object.
     */
    const T &get_opt () const {
        return detail::optmgr_kv_impl_t<T>::get_opt ();
    }

    /*! Put
     *
     * \param k        Key string
     * \param v        Value string
     *
     * \return         0 on success; -1 on error.
     */
    int put (const std::string &k, const std::string &v) {
        return detail::optmgr_kv_impl_t<T>::put (k, v);
    }

    /*! Put
     *
     * \param kv       Key=Value string
     *
     * \return         0 on success; -1 on error.
     */
    int put (const std::string &kv) {
        return detail::optmgr_kv_impl_t<T>::put (kv);
    }

    /*! Get
     *
     * \param k        Key string
     * \param v        Returning value string associated with the key
     *
     * \return         0 on success; -1 on error.
     */
    int get (const std::string &k, std::string &v) const {
        return detail::optmgr_kv_impl_t<T>::get (k, v);
    }

    /*! Parse key=value option pairs and update the state of
     *  the option-set of T type. If T doesn't have parse() method,
     *  compilation error will ensue.
     *
     * \param info     parse warning or error string.
     * \return         0 on success; -1 on error.
     */
    int parse (std::string &info) {
        return detail::optmgr_kv_impl_t<T>::parse (info);
    }
};


/*! Parsing utilities.
 *
 */
struct optmgr_parse_t : public detail::optmgr_parse_impl_t {

    /*! Parse a string that contains key and value delimited with token.
     *  The parsed key and value are passed through k and v respectively.
     *
     * \param str      String that contains key and value.
     * \param token    Token string.
     * \param k        Key string to return.
     * \param v        Value string to return.
     *
     * \return         0 on success; -1 on error.
     *                 errno: EINVAL and EPROTO
     */
    int parse_single (const std::string &str, const std::string &token,
                      std::string &k, std::string &v) {
        return detail::optmgr_parse_impl_t::parse_single (str, token, k, v);
    }

    /*! Parse multi_value string: each value is delimited with the
     *  delim character and return the parsed results to entries.
     *  (e.g., "foo1=bar1 foo2=bar2" whereby ' ' is the delim character
     *  then each of foo*=bar* pairs is parsed into the entries vector).
     *
     * \param multi_value
     *                 Multi-value string.
     * \param delim    Delim character.
     * \param entries  Parsed results are returned to vector of strings.
     *
     * \return         0 on success; -1 on error.
     *                 errno: ENOMEM
     */
    int parse_multi (const std::string multi_value, const char delim,
                     std::vector<std::string> &entries) {
        return detail::optmgr_parse_impl_t::parse_multi (multi_value,
                                                         delim, entries);
    }

    /*! Parse the m_opts string that contains multiple options delimited
     *  with odelim.
     *  (e.g., "foo1=bar1 foo2=bar2" whereby ' ' is the option delimiter
     *  and '=' is the key-value delimiter, the parsed result
     *  will be returned to std::map accordingly.
     *
     * \param m_opts   Multi-option string.
     * \param odelim   Option delimiter character.
     * \param kdelim   Key-value delimiter character.
     * \param opt_mp   std::map object that contains the parsed result.
     *
     * \return         0 on success; -1 on error.
     *                 errno: ENOMEM, EEXIST, EINVAL and EPROTO.
     */
    int parse_multi_options (const std::string &m_opts, const char odelim,
                             const char kdelim, std::map<std::string,
                                                         std::string> &opt_mp) {
        return detail::optmgr_parse_impl_t::parse_multi_options (m_opts, odelim,
                                                                 kdelim, opt_mp);
    }
};

} // namespace Flux::opts_manager
} // namespace Flux

#endif // OPTMGR_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
