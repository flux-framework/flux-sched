--/***************************************************************************\
--  Copyright (c) 2014 Lawrence Livermore National Security, LLC.  Produced at
--  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
--  LLNL-CODE-658032 All rights reserved.
--
--  This file is part of the Flux resource manager framework.
--  For details, see https://github.com/flux-framework.
--
--  This program is free software; you can redistribute it and/or modify it
--  under the terms of the GNU General Public License as published by the Free
--  Software Foundation; either version 2 of the license, or (at your option)
--  any later version.
--
--  Flux is distributed in the hope that it will be useful, but WITHOUT
--  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
--  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
--  GNU General Public License for more details.
--
--  You should have received a copy of the GNU General Public License along
--  with this program; if not, write to the Free Software Foundation, Inc.,
--  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
--  See also:  http://www.gnu.org/licenses/
--\***************************************************************************/

local URI = {}

local function uri_create (s)
    assert (s, "Failed to pass string to uri_create")
    local uri = {}
    uri.name = s:match ("(%S+):")
    if not uri.name then
        uri.name = s
    else
        uri.path = s:match (":(%S+)$")
    end
    return setmetatable (uri, URI)
end

function URI.__index (uri, key)
    if key == "parent" then
        local path =  uri.path:match ("(%S+)/[^/]+") or "/"
        return uri_create (uri.name..":"..path)
    elseif key == "basename" then
        local b = uri.path:match ("/([^/]+)$")
        return b
    end
    return nil
end

function URI:__tostring ()
    return self.name..":"..(self.path or "/")
end

return { new = uri_create }

-- vi: ts=4 sw=4 expandtab
