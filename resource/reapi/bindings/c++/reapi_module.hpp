/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef REAPI_MODULE_HPP
#define REAPI_MODULE_HPP

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>
}

#include <cstdint>
#include <string>
#include "resource/reapi/bindings/c++/reapi.hpp"
#include "resource/policies/base/match_op.h"

namespace Flux {
namespace resource_model {
namespace detail {

class reapi_module_t : public reapi_t {
   public:
    static int match_allocate (void *h,
                               match_op_t match_op,
                               const std::string &jobspec,
                               const uint64_t jobid,
                               bool &reserved,
                               std::string &R,
                               int64_t &at,
                               double &ov,
                               int64_t within = -1);
    static int match_allocate (void *h,
                               bool orelse_reserve,
                               const std::string &jobspec,
                               const uint64_t jobid,
                               bool &reserved,
                               std::string &R,
                               int64_t &at,
                               double &ov,
                               int64_t within = -1);
    static int match_allocate_multi (void *h,
                                     bool orelse_reserve,
                                     const char *jobs,
                                     queue_adapter_base_t *adapter);
    static int match_allocate_multi (void *h,
                                     match_op_t match_op,
                                     const char *jobs,
                                     queue_adapter_base_t *adapter);
    static int update_allocate (void *h,
                                const uint64_t jobid,
                                const std::string &R,
                                int64_t &at,
                                double &ov,
                                std::string &R_out);
    static int cancel (void *h, const uint64_t jobid, bool noent_ok);
    static int cancel (void *h,
                       const uint64_t jobid,
                       const std::string &R,
                       bool noent_ok,
                       bool &full_removal);
    static int info (void *h, const uint64_t jobid, bool &reserved, int64_t &at, double &ov);
    static int stat (void *h,
                     int64_t &V,
                     int64_t &E,
                     int64_t &J,
                     double &load,
                     double &min,
                     double &max,
                     double &avg);
};

}  // namespace detail
}  // namespace resource_model
}  // namespace Flux

#endif  // REAPI_MODULE_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
