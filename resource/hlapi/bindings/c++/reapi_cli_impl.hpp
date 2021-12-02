/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef REAPI_CLI_IMPL_HPP
#define REAPI_CLI_IMPL_HPP

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>
}

#include "resource/hlapi/bindings/c++/reapi_cli.hpp"

namespace Flux {
namespace resource_model {
namespace detail {

const int NOT_YET_IMPLEMENTED = -1;

/****************************************************************************
 *                                                                          *
 *               REAPI CLI Class Private Definitions                        *
 *                                                                          *
 ****************************************************************************/

std::string reapi_cli_t::m_err_msg = "";

/****************************************************************************
 *                                                                          *
 *            REAPI CLI Class Public API Definitions                        *
 *                                                                          *
 ****************************************************************************/

int reapi_cli_t::match_allocate (void *h, bool orelse_reserve,
                                 const std::string &jobspec,
                                 const uint64_t jobid, bool &reserved,
                                 std::string &R, int64_t &at, double &ov)
{
    return NOT_YET_IMPLEMENTED;
}

int reapi_cli_t::update_allocate (void *h, const uint64_t jobid,
                                  const std::string &R, int64_t &at, double &ov,
                                  std::string &R_out)
{
    return NOT_YET_IMPLEMENTED;
}

int reapi_cli_t::match_allocate_multi (void *h, bool orelse_reserve,
                                       const char *jobs,
                                       queue_adapter_base_t *adapter)
{
    return NOT_YET_IMPLEMENTED;
}

int reapi_cli_t::cancel (void *h, const int64_t jobid, bool noent_ok)
{
    return NOT_YET_IMPLEMENTED;
}

int reapi_cli_t::info (void *h, const int64_t jobid,
                       bool &reserved, int64_t &at, double &ov)
{
    return NOT_YET_IMPLEMENTED;
}

int reapi_cli_t::stat (void *h, int64_t &V, int64_t &E,int64_t &J,
                       double &load, double &min, double &max, double &avg)
{
    return NOT_YET_IMPLEMENTED;
}

const std::string &reapi_cli_t::get_err_message ()
{
    return m_err_msg;
}

void reapi_cli_t::clear_err_message ()
{
    m_err_msg = "";
}


} // namespace Flux::resource_model::detail
} // namespace Flux::resource_model
} // namespace Flux

#endif // REAPI_CLI_IMPL_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
