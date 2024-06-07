/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef COLOR_H
#define COLOR_H

#include <cstdint>

namespace Flux {
namespace resource_model {

class color_t {
   public:
    enum class color_offset_t : uint64_t {
        WHITE_OFFSET = 0,
        GRAY_OFFSET = 1,
        BLACK_OFFSET = 2,
        NEW_BASE = 3
    };

    // compiler will generate correct constructors and destructor

    void reset ();

    bool is_white (uint64_t c) const;
    bool is_gray (uint64_t c) const;
    bool is_black (uint64_t c) const;

    uint64_t white () const;
    uint64_t gray () const;
    uint64_t black () const;

   private:
    uint64_t m_color_base = 0;
};

}  // namespace resource_model
}  // namespace Flux

#endif  // COLOR_H

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
