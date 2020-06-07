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

#include "resource/schema/resource_data.hpp"

namespace Flux {
namespace resource_model {


/****************************************************************************
 *                                                                          *
 *                     Resource Pool Method Definitions                     *
 *                                                                          *
 ****************************************************************************/

resource_pool_t::resource_pool_t ()
{

}

resource_pool_t::resource_pool_t (const resource_pool_t &o)
{
    type = o.type;
    paths = o.paths;
    basename = o.basename;
    name = o.name;
    properties = o.properties;
    id = o.id;
    uniq_id = o.uniq_id;
    rank = o.rank;
    size = o.size;
    unit = o.unit;
    schedule = o.schedule;
    idata = o.idata;
}

resource_pool_t &resource_pool_t::operator= (const resource_pool_t &o)
{
    type = o.type;
    paths = o.paths;
    basename = o.basename;
    name = o.name;
    properties = o.properties;
    id = o.id;
    uniq_id = o.uniq_id;
    rank = o.rank;
    size = o.size;
    unit = o.unit;
    schedule = o.schedule;
    idata = o.idata;
    return *this;
}

resource_pool_t::~resource_pool_t ()
{

}


/****************************************************************************
 *                                                                          *
 *                     Resource Relation Method Definitions                 *
 *                                                                          *
 ****************************************************************************/

resource_relation_t::resource_relation_t ()
{

}

resource_relation_t::resource_relation_t (const resource_relation_t &o)
{
    name = o.name;
    idata = o.idata;
}

resource_relation_t &resource_relation_t::operator= (
                                              const resource_relation_t &o)
{
    name = o.name;
    idata = o.idata;
    return *this;
}

resource_relation_t::~resource_relation_t ()
{

}

const resource_pool_t::string_to_status resource_pool_t::str_to_status = 
      { { "up", resource_pool_t::status_t::UP }, 
      { "down", resource_pool_t::status_t::DOWN } };


} // Flux::resource_model
} // Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
