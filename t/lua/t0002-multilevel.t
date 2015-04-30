#!/usr/bin/lua
--
--  Basic rdl testing. Multi-level hierarchy
--
local t = require 'fluxometer'.init (...)
local RDL = require_ok ('RDL')

local rdl, err = RDL.eval ([[
uses "Node"

 Hierarchy "default" {
   Resource{ "cluster", name = "foo",
             children = {
               ListOf{ Node, ids="1-4",
                 args = { name = "bar", sockets = { "0-1", "2-3" } }
               }
	     }
   }
 }
]])

type_ok (rdl, 'table', "load RDL")
is (err, nil, "error is nil")

local r, err = rdl:resource ("default")
type_ok (r, 'table', "get handle to top of default hierarchy")
is (r.name, "foo", "Got a handle to resource default:/foo")

local agg = { cluster = 1, node = 4, socket = 8, core = 16 }
is_deeply (r:aggregate(), agg, "RDL aggregate checks out")

-- iterate over children
for c in r:children () do
    t:say ("child "..c.uri)
    type_ok (c, 'table', "Got a valid child")
    is (c.type, 'node',  "child is a Node")
    is (c.basename, 'bar', "child has basename bar")
    like (c.name, 'bar%d', "child name is "..c.name)
end

-- Now get a node and check sockets:
local r, err = rdl:resource ("default:/foo/bar1")

-- iterate over children
for c in r:children () do
    t:say ("child "..c.uri)
    type_ok (c, 'table', "Got a valid child")
    is (c.type, 'socket',  "child is a "..c.type)
    is (c.basename, 'socket', "child has basename "..c.basename)
    like (c.name, 'socket%d', "child name is "..c.name)
end

-- Now get a socket and check cores
local r, err = rdl:resource ("default:/foo/bar1/socket0")

for c in r:children () do
    t:say ("child "..c.uri)
    type_ok (c, 'table', "Got a valid child")
    is (c.type, 'core',  "child is a "..c.type)
    is (c.basename, 'core', "child has basename "..c.basename)
    like (c.name, 'core%d', "child name is "..c.name)
end



done_testing ()

-- vi: ts=4 sw=4 expandtab
