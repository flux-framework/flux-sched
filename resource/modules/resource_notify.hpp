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

#include <jansson.hpp>
#include <utility>

namespace Flux {
namespace resource_notify {

#define NOTIFY_REQUEST_KEY "requested"
#define NOTIFY_RESOURCES_KEY "resources"
#define NOTIFY_UP_KEY "up"
#define NOTIFY_DOWN_KEY "down"
#define NOTIFY_SHRINK_KEY "shrink"
#define NOTIFY_EXPIRATION_KEY "expiration"
#define NOTIFY_ADD_SUBGRAPH_KEY "add_subgraph"
#define NOTIFY_REMOVE_SUBGRAPH_KEY "remove_subgraph"

enum notify_flag_t : uint64_t {
    NOTIFY_NONE = 0,
    NOTIFY_RESOURCES = (1 << 0),
    NOTIFY_UP = (1 << 1),
    NOTIFY_DOWN = (1 << 2),
    NOTIFY_SHRINK = (1 << 3),
    NOTIFY_EXPIRATION = (1 << 4),
    NOTIFY_ADD_SUBGRAPH = (1 << 5),
    NOTIFY_REMOVE_SUBGRAPH = (1 << 6),
};

/*! Encode notification flags as a json object.
 *  \param flags    Input notification flags.
 *  \return         Encoded json object; nullptr on error.
 */
inline json_t *notify_flags_to_json (const std::underlying_type<notify_flag_t>::type flags)
{
    return json_pack ("{s:b,s:b,s:b,s:b,s:b,s:b,s:b}",
                      NOTIFY_RESOURCES_KEY,
                      flags & NOTIFY_RESOURCES,
                      NOTIFY_UP_KEY,
                      flags & NOTIFY_UP,
                      NOTIFY_DOWN_KEY,
                      flags & NOTIFY_DOWN,
                      NOTIFY_SHRINK_KEY,
                      flags & NOTIFY_SHRINK,
                      NOTIFY_EXPIRATION_KEY,
                      flags & NOTIFY_EXPIRATION,
                      NOTIFY_ADD_SUBGRAPH_KEY,
                      flags & NOTIFY_ADD_SUBGRAPH,
                      NOTIFY_REMOVE_SUBGRAPH_KEY,
                      flags & NOTIFY_REMOVE_SUBGRAPH);
}

/*! Decode notification flags from a json object.
 *  /param json     Input json object.
 *  \return         Decoded notify enum; NOTIFY_NONE when json is NULL.
 */
inline notify_flag_t notify_flags_from_json (json_t *json)
{
    std::underlying_type<notify_flag_t>::type flags = NOTIFY_NONE;

    if (json) {
        if (json_is_true (json_object_get (json, NOTIFY_RESOURCES_KEY)))
            flags |= NOTIFY_RESOURCES;
        if (json_is_true (json_object_get (json, NOTIFY_UP_KEY)))
            flags |= NOTIFY_UP;
        if (json_is_true (json_object_get (json, NOTIFY_DOWN_KEY)))
            flags |= NOTIFY_DOWN;
        if (json_is_true (json_object_get (json, NOTIFY_SHRINK_KEY)))
            flags |= NOTIFY_SHRINK;
        if (json_is_true (json_object_get (json, NOTIFY_EXPIRATION_KEY)))
            flags |= NOTIFY_EXPIRATION;
        if (json_is_true (json_object_get (json, NOTIFY_ADD_SUBGRAPH_KEY)))
            flags |= NOTIFY_ADD_SUBGRAPH;
        if (json_is_true (json_object_get (json, NOTIFY_REMOVE_SUBGRAPH_KEY)))
            flags |= NOTIFY_REMOVE_SUBGRAPH;
    }

    return static_cast<notify_flag_t> (flags);
}

}  // namespace resource_notify
}  // namespace Flux

#endif  // RESOURCE_NOTIFY_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
