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

} // resource_model
} // Flux

#endif // COLOR_H

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
