/*****************************************************************************\
 * Copyright 2020 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/
#ifndef OPTMGR_HPP
#define OPTMGR_HPP

#include <map>
#include <string>
#include <sstream>
#include <vector>

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
template<class T>
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
    T &operator+= (const T &o)
    {
        return m_opt += o;
    }

    /*!
     * Canonicalize the option set -- calls the same named method
     * of the object of the option set class T which canonicalizes
     * its internal state after all the composition completes.
     */
    T &canonicalize ()
    {
        return m_opt.canonicalize ();
    }

    /*!
     * Getter
     *
     * \return         Option set object of type T
     */
    const T &get_opt () const
    {
        return m_opt;
    }

    /*!
     * Return the set option parameters as an JSON string
     *
     *  \param json_out  output JSON string
     *  \return          0 on success; -1 on error.
     */
    int jsonify (std::string &json_out) const
    {
        return m_opt.jsonify (json_out);
    }

   private:
    T m_opt;
};

/*! A key value store class that helps parsing and holding
 *  the option set from a single source (e.g., an option set passed from
 *  module-load time).
 *  Template class T should be a module option-set management class
 *  that provides a "parse" method.  This method must take in an option
 *  key-value pair from the optmgr_kv_t class and update its state.
 *  In addition, T must provide "operator()" method to give specific
 *  iteration order on the option set.
 */
template<class T>
struct optmgr_kv_t {
    /*! Getter
     *
     * \return         Option set object of type T of "this" object.
     */
    const T &get_opt () const
    {
        return m_opt;
    }

    /*! Put
     *
     * \param k        Key string
     * \param v        Value string
     *
     * \return         0 on success; -1 on error.
     */
    int put (const std::string &k, const std::string &v)
    {
        int rc = 0;
        try {
            auto ret = m_kv.insert (std::pair<std::string, std::string> (k, v));
            if (!ret.second) {
                errno = EEXIST;
                rc = -1;
            }
        } catch (std::bad_alloc &) {
            errno = ENOMEM;
            rc = -1;
        }
        return rc;
    }

    /*! Put
     *
     * \param kv       Key=Value string
     *
     * \return         0 on success; -1 on error.
     */
    int put (const std::string &kv)
    {
        size_t found = std::string::npos;
        if ((found = kv.find_first_of ("=")) == std::string::npos) {
            errno = EPROTO;
            return -1;
        }
        return put (kv.substr (0, found), kv.substr (found + 1));
    }

    /*! Get
     *
     * \param k        Key string
     * \param v        Returning value string associated with the key
     *
     * \return         0 on success; -1 on error.
     */
    int get (const std::string &k, std::string &v) const
    {
        int rc = 0;
        try {
            v = m_kv.at (k);
        } catch (std::bad_alloc &) {
            errno = ENOMEM;
            rc = -1;
        } catch (std::out_of_range &) {
            errno = ENOENT;
            rc = -1;
        }
        return rc;
    }

    /*! Parse key=value option pairs and update the state of
     *  the option-set of T type. If T doesn't have parse() method,
     *  compilation error will ensue.
     *
     * \param info     parse warning or error string.
     * \return         0 on success; -1 on error.
     */
    int parse (std::string &info)
    {
        int rc = 0;
        for (const auto &kv : m_kv) {
            // If T doesn't have parse method, this produces an compiler error
            if ((rc = m_opt.parse (kv.first, kv.second, info)) < 0) {
                return rc;
            }
        }
        return rc;
    }

   private:
    T m_opt;
    std::map<std::string, std::string, T> m_kv;
};

/*! Parsing utilities.
 *
 */
struct optmgr_parse_t {
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
    int parse_single (const std::string &str,
                      const std::string &token,
                      std::string &k,
                      std::string &v)
    {
        size_t found;
        if (str == "" || token == "") {
            errno = EINVAL;
            return -1;
        }
        if ((found = str.find_first_of (token)) == std::string::npos) {
            errno = EPROTO;
            return -1;
        }
        k = str.substr (0, found);
        v = str.substr (found + 1);
        return 0;
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
    int parse_multi (const std::string multi_value,
                     const char delim,
                     std::vector<std::string> &entries)
    {
        int rc = 0;
        try {
            std::stringstream ss;
            std::string entry;
            ss << multi_value;
            while (getline (ss, entry, delim))
                entries.push_back (entry);
        } catch (std::bad_alloc &) {
            errno = ENOMEM;
            rc = -1;
        }
        return rc;
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
    int parse_multi_options (const std::string &m_opts,
                             const char odelim,
                             const char kdelim,
                             std::map<std::string, std::string> &opt_mp)
    {
        int rc = 0;
        try {
            std::vector<std::string> entries;
            if ((rc = parse_multi (m_opts, odelim, entries)) < 0)
                goto done;
            for (const auto &entry : entries) {
                std::string n = "";
                std::string v = "";
                if ((rc = parse_single (entry, std::string (1, kdelim), n, v)) < 0)
                    goto done;
                auto ret = opt_mp.insert (std::pair<std::string, std::string> (n, v));
                if (!ret.second) {
                    errno = EEXIST;
                    rc = -1;
                    goto done;
                }
            }
        } catch (std::bad_alloc &) {
            errno = ENOMEM;
            rc = -1;
        }
    done:
        return rc;
    }
};

}  // namespace opts_manager
}  // namespace Flux

#endif  // OPTMGR_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
