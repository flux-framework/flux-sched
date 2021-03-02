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

#ifndef RESOURCE_NAMESPACE_REMAPPER_HPP
#define RESOURCE_NAMESPACE_REMAPPER_HPP

#include <map>
#include <string>
#include <cstdint>

namespace Flux {
namespace resource_model {

class distinct_range_t {
public:
    explicit distinct_range_t (uint64_t point);
    distinct_range_t (uint64_t lo, uint64_t hi);

    uint64_t get_low () const;
    uint64_t get_high () const;
    bool is_point () const;
    bool operator< (const distinct_range_t &o) const;
    bool operator== (const distinct_range_t &o) const;
    bool operator!= (const distinct_range_t &o) const;

    static int get_low_high (const std::string &exec_target_range,
                             uint64_t &low, uint64_t &high);

private:
    uint64_t m_low;
    uint64_t m_high;
};


class resource_namespace_remapper_t {
public:
    int add (const uint64_t exec_target_high,
             const uint64_t exec_target_low,
             const std::string &name_type,
             uint64_t ref_id, uint64_t remapped_id);
    int add (const std::string &exec_target_range,
             const std::string &name_type,
             uint64_t ref_id, uint64_t remapped_id);
    int add_exec_target_range (
            const std::string &exec_target_range,
            const distinct_range_t &remapped_exec_target_range);
    int query (const uint64_t exec_target,
               const std::string &name_type,
               uint64_t ref_id, uint64_t &remapped_id_out) const;
    int query_exec_target (const uint64_t exec_target,
                           uint64_t &remapped_exec_target) const;
    bool is_remapped () const;

private:

    int get_low_high (const std::string &exec_target_range,
                      uint64_t &low, uint64_t &high) const;

    std::map<const distinct_range_t,
             std::map<const std::string,
                      std::map<uint64_t, uint64_t>>> m_remap;
};

} // namespace resource_model
} // namespace Flux


#endif // RESOURCE_NAMESPACE_REMAPPER_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
