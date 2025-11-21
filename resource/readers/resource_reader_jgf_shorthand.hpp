/*****************************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef RESOURCE_READER_JGF_SHORTHAND_HPP
#define RESOURCE_READER_JGF_SHORTHAND_HPP

#include "resource/readers/resource_reader_jgf.hpp"

namespace Flux {
namespace resource_model {

/*! JGF shorthand resource reader class.
 */
class resource_reader_jgf_shorthand_t : public resource_reader_jgf_t {
   public:
    virtual ~resource_reader_jgf_shorthand_t ();
};

}  // namespace resource_model
}  // namespace Flux

#endif  // RESOURCE_READER_JGF_SHORTHAND_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
