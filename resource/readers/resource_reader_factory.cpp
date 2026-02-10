/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

#include "resource/readers/resource_reader_factory.hpp"
#include "resource/readers/resource_reader_grug.hpp"
#include "resource/readers/resource_reader_hwloc.hpp"
#include "resource/readers/resource_reader_jgf.hpp"
#include "resource/readers/resource_reader_jgf_shorthand.hpp"
#include "resource/readers/resource_reader_rv1exec.hpp"

namespace Flux {
namespace resource_model {

bool known_resource_reader (const std::string &name)
{
    bool rc = false;
    if (name == "grug" || name == "hwloc" || name == "jgf" || name == "jgf_shorthand"
        || name == "rv1exec" || name == "rv1exec_force")
        rc = true;
    return rc;
}

std::shared_ptr<resource_reader_base_t> create_resource_reader (const std::string &name)
{
    std::shared_ptr<resource_reader_base_t> reader = nullptr;
    try {
        // std::make_shared has no nothrow allocator support
        if (name == "grug") {
            reader = std::make_shared<resource_reader_grug_t> ();
        } else if (name == "hwloc") {
            reader = std::make_shared<resource_reader_hwloc_t> ();
        } else if (name == "jgf") {
            reader = std::make_shared<resource_reader_jgf_t> ();
        } else if (name == "jgf_shorthand") {
            reader = std::make_shared<resource_reader_jgf_shorthand_t> ();
        } else if (name == "rv1exec" || name == "rv1exec_force") {
            reader = std::make_shared<resource_reader_rv1exec_t> ();
        } else {
            errno = EINVAL;
        }
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        reader = nullptr;
    }
    return reader;
}

}  // namespace resource_model
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
