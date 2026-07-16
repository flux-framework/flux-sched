/*****************************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef RESOURCE_NOTIFY_HPP
#define RESOURCE_NOTIFY_HPP

#include <string>

namespace Flux {
namespace resource_notify {

#define NOTIFY_RESOURCES_KEY "resources"
#define NOTIFY_UP_KEY "up"
#define NOTIFY_DOWN_KEY "down"
#define NOTIFY_SHRINK_KEY "shrink"
#define NOTIFY_EXPIRATION_KEY "expiration"

}  // namespace resource_notify
}  // namespace Flux

#endif  // RESOURCE_NOTIFY_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
