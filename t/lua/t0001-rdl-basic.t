#!/usr/bin/lua
--
--  Basic rdl testing
--
local t = require 'fluxometer'.init (...)
local RDL = require_ok ('RDL')

t:say ("Load a very simple RDL Hierarchy:\n")

local rdl, err = RDL.eval ([[
 Hierarchy "default" {
   Resource{ "foo", name = "bar", id = 0, tags = { "test_tag" } }
 }
]])

type_ok (rdl, 'table', "create rdl with single resource")
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

local tags = r.tags
type_ok (tags, 'table', "resource tags is a table")
isnt (tags.test_tag, nil, "test_tag set in resource")


-- Modify resource operations:
--
ok (r:tag ("test2"), "tag resource with 'test2'")
ok (r.tags.test2, "tag now set on resource")
ok (r:get "test2", "r:get_tag() works")
ok (r:delete_tag ("test2"), "delete tag 'test2'")
is (r.tags.test2, nil, "tag now unset on resource")

done_testing ()

-- vi: ts=4 sw=4 expandtab
