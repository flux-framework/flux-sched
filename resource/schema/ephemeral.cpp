/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

extern "C" {
#if HAVE_CONFIG_H
#include <config.h>
#endif
}

#include "resource/schema/ephemeral.hpp"

namespace Flux {
namespace resource_model {

int ephemeral_t::insert (uint64_t epoch, const std::string &key, const std::string &value)
{
    int rc = 0;

    try {
        check_and_clear_if_stale (epoch);
        m_epoch = epoch;
        auto ret = m_store.insert (std::make_pair (key, value));
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

boost::optional<std::string> ephemeral_t::get (uint64_t epoch, const std::string &key)
{
    if (check_and_clear_if_stale (epoch)) {
        return boost::none;
    }

    try {
        auto it = m_store.find (key);
        if (it == m_store.end ()) {
            return boost::none;
        }

        auto value = (*it).second;
        return value;
    } catch (const std::out_of_range &oor) {
        return boost::none;
    }
}

const std::map<std::string, std::string> &ephemeral_t::to_map (uint64_t epoch)
{
    check_and_clear_if_stale (epoch);
    return this->to_map ();
}

const std::map<std::string, std::string> &ephemeral_t::to_map () const
{
    return m_store;
}

bool ephemeral_t::check_and_clear_if_stale (uint64_t epoch)
{
    if (m_epoch < epoch) {
        // data is stale
        this->clear ();
        return true;
    }
    return false;
}

void ephemeral_t::clear ()
{
    m_store.clear ();
}

}  // namespace resource_model
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
