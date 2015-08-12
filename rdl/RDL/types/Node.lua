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

uses "Socket"

Node = Resource:subclass ('Node')
function Node:initialize (arg)
    local basename = arg.name or arg.basename
    assert (basename, "Required Node arg `name' missing")

    local id = arg.id
    assert (arg.sockets, "Required Node arg `sockets' missing")
    assert (type (arg.sockets) == "table",
            "Node argument sockets must be a table of core ids")

    Resource.initialize (self,
        { "node",
          id = id,
          name = basename,
          properties = arg.properties or {},
          tags = arg.tags or {}
        }
    )

	local num_sockets = 0
	for _,_ in pairs (arg.sockets) do
	   num_sockets = num_sockets + 1
	end

	local bw_per_socket = 0
    if arg.tags ~= nil and arg.tags['max_bw'] ~= nil then
	   bw_per_socket = arg.tags.max_bw / num_sockets
    end

    local sockid = 0
    for _,c in pairs (arg.sockets) do
        self:add_child (Socket{ id = sockid, cpus = c,
                                memory = arg.memory_per_socket,
                                tags = { ["max_bw"] = bw_per_socket, ["alloc_bw"] = 0 }})
        sockid = sockid + 1
    end
end

return Node

-- vi: ts=4 sw=4 expandtab
