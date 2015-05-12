#!/usr/bin/lua
--
--  Basic rdl testing
--
local t = require 'fluxometer'.init (...)
local RDL = require_ok ('RDL')

t:say ("Check basic functionality of default tags\n")

local rdl, err = RDL.eval ([[
 Resource:default_tags { "tag1" }
 Hierarchy "default" {
   Resource{ "foo", name = "bar", id = 0 }
 }
]])

type_ok (rdl, 'table', "create rdl with single resource and default tags")
is (err, nil, "error is nil")

local r, err = rdl:resource ("default")
type_ok (r, 'table', "get handle to resource object")

is (r.tags.tag1, 1, "Got default tag")

t:say ("Resource:default_tags should be reset for each eval")
rdl, err = RDL.eval ([[
 Hierarchy "default" {
   Resource{ "foo", name = "bar", id = 0 }
 }
]])

type_ok (rdl, 'table', "create rdl with single resource, no default tags")
is (err, nil, "error is nil")

local r, err = rdl:resource ("default")
type_ok (r, 'table', "get handle to resource object")

is (r.tags.tag1, nil, "no default tag")


t:say ("default tags work on a subclass of Resource type")
rdl, err = RDL.eval ([[
 uses "Node"
 Node:default_tags { "test" }
 Hierarchy "default" {
   Resource{ "foo", name = "bar",
    children = { Node{ name="node", id=1, sockets = { "0" } } }
  }
 }
]])

type_ok (rdl, 'table', "create rdl with generic resource and Node")
is (err, nil, "error is nil")

local r, err = rdl:resource ("default")
type_ok (r, 'table', "get handle to resource object")
is (r.tags.tag1, nil, "no default tag on Resource class")

local r, err = rdl:resource ("default:/bar/node1")
type_ok (r, 'table', "get handle to resource object")
is (err, nil, "no error")
is (r.type, "node", "got Node object as expected")
is (r.tags.test, 1, "default tag on Node class works")


t:say ("default tags inherited by Resource subclass")
rdl, err = RDL.eval ([[
 uses "Node"
 Resource:default_tags { "test" }
 Hierarchy "default" {
   Resource{ "foo", name = "bar",
    children = { Node{ name="node", id=1, sockets = { "0" } } }
  }
 }
]])

local r, err = rdl:resource ("default:/bar/node1")
type_ok (r, 'table', "get handle to resource object")
is (err, nil, "no error")
is (r.type, "node", "got Node object as expected")
is (r.tags.test, 1, "default tag on Node class works")


t:say ("Override default_tags on Resource subclass")
rdl, err = RDL.eval ([[
 uses "Node"
 Resource:default_tags { "test" }
 Node:default_tags { "test2" }
 Hierarchy "default" {
   Resource{ "foo", name = "bar",
    children = { Node{ name="node", id=1, sockets = { "0" } } }
  }
 }
]])

local r, err = rdl:resource ("default:/bar/node1")
type_ok (r, 'table', "get handle to resource object")
is (err, nil, "no error")
is (r.type, "node", "got Node object as expected")
is (r.tags.test, nil, "first  tag on Node class not set")
is (r.tags.test2, 1, "overridden tag on Node class is set")

--
t:say ("Override default tags with empty list")
rdl, err = RDL.eval ([[
 uses "Node"
 Resource:default_tags { "test" }
 Node:default_tags {}
 Hierarchy "default" {
   Resource{ "foo", name = "bar",
    children = { Node{ name="node", id=1, sockets = { "0" } } }
  }
 }
]])

local r, err = rdl:resource ("default:/bar/node1")
type_ok (r, 'table', "get handle to resource object")
is (err, nil, "no error")
is (r.type, "node", "got Node object as expected")
is_deeply (r.tags, {}, "No tags on node instance")

local r, err = rdl:resource ("default:/bar")
type_ok (r, 'table', "get handle to resource object")
is (err, nil, "no error")
is (r.tags.test, 1, "Tag on resource object still set")

--
t:say ("Set default tags on 'uses' line")
rdl, err = RDL.eval ([[
 uses "Node" :default_tags { "set-a-tag" }
 Hierarchy "default" {
   Resource{ "foo", name = "bar",
    children = { Node{ name="node", id=1, sockets = { "0" } } }
  }
 }
]])
type_ok (rdl, 'table', "RDL eval success")
is (err, nil, "no error")

local r, err = rdl:resource ("default:/bar/node1")
type_ok (r, 'table', "get handle to resource object")
is (err, nil, "no error")
is (r.type, "node", "got Node object as expected")
is (r.tags['set-a-tag'], 1, "tag is set on node instance")

done_testing ()

-- vi: ts=4 sw=4 expandtab
