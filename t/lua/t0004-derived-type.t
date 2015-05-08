#!/usr/bin/lua
--
--  RDL Derived type testing
--
local t = require 'fluxometer'.init (...)
local RDL = require_ok ('RDL')

t:say ("Load a simple RDL Hierarchy using a derived type:\n")

local rdl, err = RDL.eval ([[

 Foo = Resource:subclass 'Foo'
 function Foo:initialize (arg)
   local name = arg.name or arg[1]
   Resource.initialize (self, { "foo", name = name, id = arg.id })
 end

 Hierarchy "default" { Foo {"bar", id = 0} }
]])

type_ok (rdl, 'table', "create rdl with single derived resource")
is (err, nil, "error is nil")

local r, err = rdl:resource ("default")
type_ok (r, 'table', "get handle to resource object")

is (err, nil, "error is nil")

is (r.name, "bar0", "resource name")
is (r.basename, "bar", "resource basename")
is (r.type, "foo", "resource type")
is (r.uri, "default:/bar0", "resource URI")
is (r.path, "/bar0", "resource path")
is (r.id, 0, "resource id number")

done_testing ()

-- vi: ts=4 sw=4 expandtab
