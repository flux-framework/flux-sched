#!/usr/bin/env lua
--
--  Basic rdl testing. Multi-hierarchy
--
local t = require 'fluxometer'.init (...)
local RDL = require_ok ('RDL')

local rdl, err = RDL.eval ([[
uses "Node"

 Hierarchy "default" {
   Resource{ "cluster", name = "foo",
             children = {
               ListOf{ Node, ids="1-4",
                 args = { basename = "bar", sockets = { "0-1", "2-3" } }
               }
	     }
   }
 }
 Hierarchy "power" {
   Resource{ "powerpanel", name="ppn1", size = 1000 }
 }
]])

type_ok (rdl, 'table', "load RDL")
is (err, nil, "error is nil")

local r, err = rdl:resource ("default")
type_ok (r, 'table', "get handle to top of default hierarchy")
is (r.name, "foo", "Got a handle to resource default:/foo")

local r, err = rdl:resource ("power")
type_ok (r, 'table', "get handle to top of power hierarchy")
is (r.name, "ppn1", "Got a handle to resource power:/ppn1")

local h = rdl:hierarchy_next ()
local r = {}
while h do
    r [h] = true
    h = rdl:hierarchy_next (h)
end

ok (r.default, "hierarchy_next got default hierarchy")
ok (r.power, "hierarchy_next got power hierarchy")

done_testing ()

-- vi: ts=4 sw=4 expandtab
