/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef RESOURCE_READER_FACTORY_HPP
#define RESOURCE_READER_FACTORY_HPP

#include <string>
#include <memory>
#include <flux/core/types.h>
#include "resource/readers/resource_reader_base.hpp"

namespace Flux {
namespace resource_model {

bool known_resource_reader (const std::string &name);
std::shared_ptr<resource_reader_base_t> create_resource_reader (const std::string &name);

/*! Map an RFC 20/40 scheduling.writer URI to the name of the reader that can
 *  parse the R it was written with.
 *
 *  Per RFC 20, writer is a URI whose scheme names the scheduler and whose path
 *  is interpreted by that scheduler.  Per RFC 40:
 *    - writer absent (writer_uri == nullptr) -> "fluxion" assumed -> "jgf"
 *    - "fluxion" (no path)                   -> path assumed "jgf" -> "jgf"
 *    - "fluxion:jgf"                         -> "jgf"
 *    - "fluxion:jgf_shorthand"               -> "jgf_shorthand"
 *  A non-fluxion scheme (another scheduler's data) or an unsupported fluxion
 *  path is not something we can read.
 *
 *  Note: the caller handles the case where scheduling is absent entirely, which
 *  per RFC 20 is bare Rv1 and uses the "rv1exec" reader.
 *
 *  \param writer_uri  value of scheduling.writer, or nullptr if the key is absent
 *  \param errp        optional; on error, filled with a human-readable message
 *  \return            reader name, or an empty string with errno=EINVAL if the
 *                     writer is unrecognized or unsupported
 */
std::string reader_name_from_writer (const char *writer_uri, flux_error_t *errp = nullptr);

}  // namespace resource_model
}  // namespace Flux

#endif  // RESOURCE_READER_FACTORY_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
