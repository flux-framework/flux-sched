/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef SYSTEM_DEFAULT_HPP
#define SYSTEM_DEFAULT_HPP

// FIXME: These need to be converted into a resource configuration file
namespace Flux {
namespace resource_model {
namespace detail {
const int64_t SYSTEM_DEFAULT_DURATION = 43200;   // 12 hours
const int64_t SYSTEM_MAX_DURATION = 3153600000;  // 100 years
}  // namespace detail
}  // namespace resource_model
}  // namespace Flux

#endif  // SYSTEM_DEFAULT_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
