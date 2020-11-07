/*****************************************************************************\
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

#include "resource/schema/ephemeral.hpp"

namespace Flux {
namespace resource_model {

int ephemeral_t::insert (uint64_t epoch,
                         const std::string &key,
                         const std::string &value)
{
    int rc = 0;

    try {
        check_and_clear_if_stale (epoch);
        m_epoch = epoch;
        auto ret = m_store.insert(std::make_pair (key, value));
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
    } catch (const std::out_of_range& oor) {
        return boost::none;
    }
}



const std::map<std::string, std::string>& ephemeral_t::to_map (uint64_t epoch)
{
    check_and_clear_if_stale (epoch);
    return this->to_map ();
}

const std::map<std::string, std::string>& ephemeral_t::to_map () const
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


} // resource_model
} // Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
