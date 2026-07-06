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

#include <cstdio>
#include "resource/readers/resource_reader_factory.hpp"
#include "resource/readers/resource_reader_grug.hpp"
#include "resource/readers/resource_reader_hwloc.hpp"
#include "resource/readers/resource_reader_jgf.hpp"
#include "resource/readers/resource_reader_jgf_shorthand.hpp"
#include "resource/readers/resource_reader_rv1exec.hpp"

namespace Flux {
namespace resource_model {

namespace {
void set_error (flux_error_t *errp, const char *msg)
{
    if (errp)
        snprintf (errp->text, sizeof (errp->text), "%s", msg);
}
}  // namespace

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

std::string reader_name_from_writer (const char *writer_uri, flux_error_t *errp)
{
    // RFC 40: writer absent, or "fluxion" with no path, defaults to "jgf".
    if (writer_uri == nullptr)
        return "jgf";

    std::string uri (writer_uri);
    // A writer URI's scheme names the producing scheduler (RFC 20).  We can
    // only interpret data written by fluxion.
    std::string scheme, path;
    auto colon = uri.find (':');
    if (colon == std::string::npos) {
        scheme = uri;
    } else {
        scheme = uri.substr (0, colon);
        path = uri.substr (colon + 1);
    }
    if (scheme != "fluxion") {
        set_error (errp, "unsupported scheduling.writer scheme (not fluxion)");
        errno = EINVAL;
        return "";
    }
    // Map the fluxion-defined path to the reader that parses it.  An empty path
    // ("fluxion" or "fluxion:") defaults to jgf per RFC 40.
    if (path.empty () || path == "jgf")
        return "jgf";
    if (path == "jgf_shorthand")
        return "jgf_shorthand";

    set_error (errp, "unsupported fluxion scheduling.writer format");
    errno = EINVAL;
    return "";
}

}  // namespace resource_model
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
