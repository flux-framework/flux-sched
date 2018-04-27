/*****************************************************************************\
 *  LLNL-CODE-658032 All rights reserved.
 *
 *  This file is part of the Flux resource manager framework.
 *  For details, see https://github.com/flux-framework.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license, or (at your option)
 *  any later version.
 *
 *  Flux is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *  See also:  http://www.gnu.org/licenses/
\*****************************************************************************/

#include "color.hpp"

namespace Flux {
namespace resource_model {

void color_t::reset ()
{
    m_color_base += static_cast<uint64_t>(color_offset_t::NEW_BASE);
}

bool color_t::is_white (uint64_t c) const
{
    return c <= (m_color_base
                 + static_cast<uint64_t>(color_offset_t::WHITE_OFFSET));
}

bool color_t::is_gray (uint64_t c) const
{
    return c == (m_color_base
                 + static_cast<uint64_t>(color_offset_t::GRAY_OFFSET));
}

bool color_t::is_black (uint64_t c) const
{
    return c == (m_color_base
                 + static_cast<uint64_t>(color_offset_t::BLACK_OFFSET));
}

uint64_t color_t::white () const
{
    return m_color_base
           + static_cast<uint64_t>(color_offset_t::WHITE_OFFSET);
}

uint64_t color_t::gray () const
{
    return m_color_base
           + static_cast<uint64_t>(color_offset_t::GRAY_OFFSET);
}

uint64_t color_t::black () const
{
    return m_color_base
           + static_cast<uint64_t>(color_offset_t::BLACK_OFFSET);
}

} // resource_model
} // Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
