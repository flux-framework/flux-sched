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

local ResourceData = require "RDL.ResourceData"
local class = require "middleclass"

---
-- A Resource is a hierarchical reference to ResourceData
--
local Resource = class ('Resource')

Resource._defaultTags = {}

-- Set a list of default tags for this class:
function Resource:default_tags (t)
    local tags = {}
    for k,v in pairs (t) do
        if type(v) == 'table' then error ("tags value cannot be a table!") end
        tags[k] = v
    end
    self._defaultTags = tags
end

function Resource:initialize (args)
    self.children = {}
    if args.children then
        for _,r in pairs (args.children) do
            self:add_child (r)
        end
        args.children = nil
    end

    if not args.tags then
        args.tags = {}
    end

    --
    -- Set default tags from the _defaultTags class attribute.
    --  (This table is inherited by subclasses, so setting default
    --   tags for the "Resource" class causes all resources to get
    --   those tags, unless overridden in a subclass)
    --
    for k,v in pairs (self.class._defaultTags) do
        if not args.tags[k] then
            args.tags[k] = v
        end
    end

    self.resource = ResourceData (args)
end

function Resource:add_child (r)
    table.insert (self.children, r)
    r.parent = self
    return self
end

function Resource:children ()
    return pairs(self.children)
end

function Resource:__tostring ()
    return tostring (self.resource)
end

function Resource:__concat (x)
    return tostring (self) .. tostring (x)
end

return Resource
-- vi: ts=4 sw=4 expandtab
