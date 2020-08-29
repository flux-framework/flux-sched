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

#ifndef EPHEMERAL_H
#define EPHEMERAL_H

#include <cstdint>
#include <map>
#include <boost/optional.hpp>

namespace Flux {
namespace resource_model {

class ephemeral_t {
public:
    int insert (uint64_t epoch,
                const std::string &key,
                const std::string &value);
    boost::optional<std::string> get (uint64_t epoch, const std::string &key);
    const std::map<std::string, std::string>& to_map (uint64_t epoch);
    const std::map<std::string, std::string>& to_map () const;
    bool check_and_clear_if_stale (uint64_t epoch);
    void clear ();

private:
    std::map<std::string, std::string> m_store;
    uint64_t m_epoch;
};

} // resource_model
} // Flux

#endif // EPHEMERAL_H

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
