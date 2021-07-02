/*****************************************************************************\
 * Copyright 2020 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef OPTMGR_IMPL_HPP
#define OPTMGR_IMPL_HPP

#include <map>
#include <string>
#include <sstream>
#include <vector>

namespace Flux {
namespace opts_manager {
namespace detail {

template <class T>
class optmgr_kv_impl_t {
protected:

    const T &get_opt () const {
        return m_opt;
    }

    int put (const std::string &k, const std::string &v) {
        int rc = 0;
        try {
            auto ret = m_kv.insert (std::pair<std::string, std::string> (k, v));
            if (!ret.second) {
                errno = EEXIST;
                rc = -1;
            }
        }
        catch (std::bad_alloc &) {
            errno = ENOMEM;
            rc = -1;
        }
        return rc;
    }

    int put (const std::string &kv) {
        size_t found = std::string::npos;
        if ( (found = kv.find_first_of ("=")) == std::string::npos) {
            errno = EPROTO;
            return -1;
        }
        return put (kv.substr (0, found), kv.substr (found + 1));
    }

    int get (const std::string &k, std::string &v) const {
        int rc = 0;
        try {
            v = m_kv.at (k);
        }
        catch (std::bad_alloc &) {
            errno = ENOMEM;
            rc = -1;
        }
        catch (std::out_of_range &) {
            errno = ENOENT;
            rc = -1;
        }
        return rc;
    }

    int parse (std::string &info) {
        int rc = 0;
        for (const auto &kv : m_kv) {
            // If T doesn't have parse method, this produces an compiler error
            if ( (rc = m_opt.parse (kv.first, kv.second, info)) < 0) {
                return rc;
            }
        }
        return rc;
    }

private:
    T m_opt;
    std::map<std::string, std::string, T> m_kv;
};


class optmgr_parse_impl_t {
protected:
    int parse_single (const std::string &str, const std::string &token,
                      std::string &k, std::string &v) {
        size_t found;
        if (str == "" || token == "") {
            errno = EINVAL;
            return -1;
        }
        if ( (found = str.find_first_of (token)) == std::string::npos) {
            errno = EPROTO;
            return -1;
        }
        k = str.substr (0, found);
        v = str.substr (found + 1);
        return 0;
    }

    int parse_multi (const std::string multi_value, const char delim,
                     std::vector<std::string> &entries) {
        int rc = 0;
        try {
            std::stringstream ss;
            std::string entry;
            ss << multi_value;
            while (getline (ss, entry, delim))
                entries.push_back (entry);
        }
        catch (std::bad_alloc &) {
            errno = ENOMEM;
            rc = -1;
        }
        return rc;
    }

    int parse_multi_options (const std::string &m_opts, const char odelim,
                             const char kdelim, std::map<std::string,
                                                         std::string> &opt_mp) {
        int rc = 0;
        try {
            std::vector<std::string> entries;
            if ( (rc = parse_multi (m_opts, odelim, entries)) < 0)
                goto done;
            for (const auto &entry : entries) {
                std::string n = "";
                std::string v = "";
                if ( (rc = parse_single (entry, std::string (1, kdelim), n, v)) < 0)
                    goto done;
                auto ret = opt_mp.insert (std::pair<std::string,
                                                    std::string> (n, v));
                if (!ret.second) {
                    errno = EEXIST;
                    rc = -1;
                    goto done;
                }
            }
        }
        catch (std::bad_alloc &) {
            errno = ENOMEM;
            rc = -1;
        }
    done:
        return rc;
    
        }
};


} // detail
} // opts_manager
} // Flux

#endif // OPTMGR_IMPL_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
