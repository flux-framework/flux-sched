/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
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
    int insert (uint64_t epoch, const std::string &key, const std::string &value);
    boost::optional<std::string> get (uint64_t epoch, const std::string &key);
    const std::map<std::string, std::string> &to_map (uint64_t epoch);
    const std::map<std::string, std::string> &to_map () const;
    bool check_and_clear_if_stale (uint64_t epoch);
    void clear ();

   private:
    std::map<std::string, std::string> m_store;
    // Need to initialize; Valgrind reports errors.
    uint64_t m_epoch = 0;
};

}  // namespace resource_model
}  // namespace Flux

#endif  // EPHEMERAL_H

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
