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

Socket = Resource:subclass ('Socket')

function Socket:initialize (arg)
    local cpuset = require 'cpuset'.new

    assert (tonumber(arg.id),   "Required Socket arg `id' missing")
    assert (type(arg.cpus) == "string", "Required Socket arg `cpus' missing")

    Resource.initialize (self,
        { "socket",
          id = arg.id,
          properties = { cpus = arg.cpus },
		  tags = arg.tags or {}
        }
    )
    --
    -- Add all child cores:
    --
    local id = 0
    local cset = cpuset (arg.cpus)

	local num_cores = 0
	for _ in cset:setbits() do
	   num_cores = num_cores + 1
	end

	local bw_per_core = 0
	if arg.tags ~=nil and arg.tags['max_bw'] ~= nil then
	   bw_per_core = arg.tags.max_bw / num_cores
	end

    for core in cset:setbits() do
        self:add_child (
            Resource{ "core", id = core, properties = { localid = id }, tags = { ["max_bw"] = bw_per_core, ["alloc_bw"] = 0 }}
        )
        id = id + 1
    end

    if arg.memory and tonumber (arg.memory) then
        self:add_child (
            Resource{ "memory", size = arg.memory }
        )
    end
end

return Socket

-- vi: ts=4 sw=4 expandtab
