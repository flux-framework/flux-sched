/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef QUEUE_SYSTEM_DEFAULT_HPP
#define QUEUE_SYSTEM_DEFAULT_HPP

namespace Flux {
namespace queue_manager {
const unsigned int MAX_QUEUE_DEPTH = 1000000;
const unsigned int DEFAULT_QUEUE_DEPTH = 32;
const unsigned int MAX_RESERVATION_DEPTH = 100000;
const unsigned int HYBRID_RESERVATION_DEPTH = 64;
}  // namespace queue_manager
}  // namespace Flux

#endif  // QUEUE_SYSTEM_DEFAULT_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
