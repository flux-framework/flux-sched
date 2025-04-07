/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
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

#include "resource/readers/resource_reader_base.hpp"
#include "resource/store/resource_graph_store.hpp"

using namespace Flux::resource_model;

////////////////////////////////////////////////////////////////////////////////
// Private Base Reader API
////////////////////////////////////////////////////////////////////////////////

bool resource_reader_base_t::in_allowlist (const std::string &resource)
{
    return allowlist.empty () || (allowlist.find (resource) != allowlist.end ());
}

////////////////////////////////////////////////////////////////////////////////
// Public Base Reader API
////////////////////////////////////////////////////////////////////////////////

resource_reader_base_t::~resource_reader_base_t ()
{
}

int resource_reader_base_t::set_allowlist (const std::string &csl)
{
    if (csl == "")
        return 0;

    int rc = -1;
    size_t pos = 0;
    std::string csl_copy = csl;
    std::string sep = ",";

    try {
        while ((pos = csl_copy.find (sep)) != std::string::npos) {
            std::string resource = csl_copy.substr (0, pos);
            if (resource != "")
                allowlist.insert (resource);
            csl_copy.erase (0, pos + sep.length ());
        }
        if (csl_copy != "")
            allowlist.insert (csl_copy);
        errno = EINVAL;
        rc = allowlist.empty () ? -1 : 0;
    } catch (std::out_of_range &e) {
        errno = EINVAL;
        rc = -1;
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        rc = -1;
    }

    return rc;
}

bool resource_reader_base_t::is_allowlist_set ()
{
    return !allowlist.empty ();
}

const std::string &resource_reader_base_t::err_message () const
{
    return m_err_msg;
}

void resource_reader_base_t::clear_err_message ()
{
    m_err_msg = "";
}

int resource_reader_base_t::split_hostname (const std::string &hn,
                                            std::string &basename,
                                            int64_t &id) const
{
    std::string suffix;
    std::size_t first;
    basename = hn;
    std::size_t last = basename.find_last_not_of ("0123456789");

    if (last == (basename.size () - 1)) {
        id = -1;
        return 0;
    }
    if (last != std::string::npos) {
        // has numeric suffix
        suffix = basename.substr (last + 1);
        basename = basename.substr (0, last + 1);
    } else {
        // all numbers
        suffix = basename;
    }

    first = suffix.find_first_not_of ("0");
    if (first == std::string::npos) {
        id = 0;  // all 0s
        return 0;
    }

    suffix = suffix.substr (first);
    try {
        id = std::stoll (suffix);
    } catch (std::invalid_argument &e) {
        errno = EINVAL;
        goto error;
    } catch (std::out_of_range &e) {
        errno = EOVERFLOW;
        goto error;
    }
    return 0;

error:
    return -1;
}

int resource_reader_base_t::get_hostname_suffix (const std::string &hn, int64_t &id) const
{
    if (hn == "") {
        errno = EINVAL;
        return -1;
    }
    std::string basename;
    return split_hostname (hn, basename, id);
}

int resource_reader_base_t::get_host_basename (const std::string &hn, std::string &basename) const
{
    if (hn == "") {
        errno = EINVAL;
        return -1;
    }
    int64_t id;
    return split_hostname (hn, basename, id);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
