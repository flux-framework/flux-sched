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

#include "resource/readers/resource_reader_base.hpp"
#include "resource/store/resource_graph_store.hpp"

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

using namespace Flux::resource_model;


/********************************************************************************
 *                                                                              *
 *                         Private Base Reader API                              *
 *                                                                              *
 ********************************************************************************/

bool resource_reader_base_t::in_whitelist (const std::string &resource)
{
    return whitelist.empty ()
           || (whitelist.find (resource) != whitelist.end ());
}


/********************************************************************************
 *                                                                              *
 *                         Public Base Reader API                               *
 *                                                                              *
 ********************************************************************************/

resource_reader_base_t::~resource_reader_base_t ()
{

}

int resource_reader_base_t::set_whitelist (const std::string &csl)
{
    if (csl == "")
        return 0;

    int rc = -1;
    size_t pos = 0;
    std::string csl_copy = csl;
    std::string sep = ",";

    try {
        while ((pos = csl_copy.find (sep)) != std::string::npos) {
            std::string resource = csl_copy.substr (0, pos);
            if (resource != "")
                whitelist.insert (resource);
            csl_copy.erase (0, pos + sep.length ());
        }
        if (csl_copy != "")
            whitelist.insert (csl_copy);
        errno = EINVAL;
        rc = whitelist.empty ()? -1 : 0;
    } catch (std::out_of_range &e) {
        errno = EINVAL;
        rc = -1;
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        rc = -1;
    }

    return rc;
}

const std::string &resource_reader_base_t::err_message () const
{
    return m_err_msg;
}

void resource_reader_base_t::clear_err_message ()
{
    m_err_msg = "";
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
