/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

extern "C" {
#if HAVE_CONFIG_H
#include <config.h>
#endif
}

#include "resource/schema/color.hpp"

namespace Flux {
namespace resource_model {

void color_t::reset ()
{
    m_color_base += static_cast<uint64_t> (color_offset_t::NEW_BASE);
}

bool color_t::is_white (uint64_t c) const
{
    return c <= (m_color_base + static_cast<uint64_t> (color_offset_t::WHITE_OFFSET));
}

bool color_t::is_gray (uint64_t c) const
{
    return c == (m_color_base + static_cast<uint64_t> (color_offset_t::GRAY_OFFSET));
}

bool color_t::is_black (uint64_t c) const
{
    return c == (m_color_base + static_cast<uint64_t> (color_offset_t::BLACK_OFFSET));
}

uint64_t color_t::white () const
{
    return m_color_base + static_cast<uint64_t> (color_offset_t::WHITE_OFFSET);
}

uint64_t color_t::gray () const
{
    return m_color_base + static_cast<uint64_t> (color_offset_t::GRAY_OFFSET);
}

uint64_t color_t::black () const
{
    return m_color_base + static_cast<uint64_t> (color_offset_t::BLACK_OFFSET);
}

}  // namespace resource_model
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
