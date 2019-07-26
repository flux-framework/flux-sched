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

#ifndef QUEUE_SYSTEM_DEFAULT_HPP
#define QUEUE_SYSTEM_DEFAULT_HPP

// FIXME: These need to be coverted into a scheduler configuration file
namespace Flux {
namespace queue_manager {
    const unsigned int MAX_QUEUE_DEPTH=16384;
    const unsigned int DEFAULT_QUEUE_DEPTH=8192;
    const unsigned int MAX_RESERVATION_DEPTH = 4096;
    const unsigned int HYBRID_RESERVATION_DEPTH = 64;
} // namespace resource_model
} // namespace Flux

#endif // QUEUE_SYSTEM_DEFAULT_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
