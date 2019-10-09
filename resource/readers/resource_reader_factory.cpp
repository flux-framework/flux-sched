/*****************************************************************************\
 *  Copyright (c) 2019 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
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

#include "resource/readers/resource_reader_factory.hpp"
#include "resource/readers/resource_reader_grug.hpp"
#include "resource/readers/resource_reader_hwloc.hpp"
#include "resource/readers/resource_reader_jgf.hpp"

namespace Flux {
namespace resource_model {

bool known_resource_reader (const std::string &name)
{
    bool rc = false;
    if (name == "grug" || name == "hwloc" || name == "jgf")
        rc = true;
    return rc;
}

std::shared_ptr<resource_reader_base_t> create_resource_reader (
                                            const std::string &name)
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
        } else {
            errno = EINVAL;
        }
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        reader = nullptr;
    }
    return reader;
}

}
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
