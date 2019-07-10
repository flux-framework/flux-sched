/*****************************************************************************\
 *  Copyright (c) 2014 Lawrence Livermore National Security, LLC.  Produced at
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

#ifndef SYSTEM_DEFAULT_HPP
#define SYSTEM_DEFAULT_HPP

// FIXME: These need to be coverted into a resource configuration file
namespace Flux {
namespace resource_model {
namespace detail {
    const uint64_t SYSTEM_DEFAULT_DURATION = 43200; // 12 hours
    const uint64_t SYSTEM_MAX_DURATION = 604800;    //  7 days
} // namespace detail
} // namespace resource_model
} // namespace Flux

#endif // SYSTEM_DEFAULT_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
