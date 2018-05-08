#!/usr/bin/env lua

local cpuset = require 'flux.cpuset'
local id_to_kvs_path = require 'wreck'.id_to_path
local f = assert (require 'flux'.new())
local jobid = tonumber (arg[1])
local rank = arg[2] or "all"
local resource_type = arg[3] or "all"
local format = arg[4] or "all"

local function die (...)
    io.stderr:write (string.format (...))
    os.exit (1)
end

local function usage ()
    io.stderr:write ('Usage: R_lite Jobid [Rank Resource Format]\n')
    io.stderr:write ([[
  Print R_lite information on a job.
      Jobid       jobid.

  Optional Arguments
      Rank        a specific rank for which to print resource info;
                  if "all" is given, print for all ranks.
      Resource    type of the resource for which to print the info:
                  all (default), core, or gpu.
      Format      if "count" is given, print only the count on the resource(s)
                  if "id", print only the ID of the resource(s)
                  if omitted, print both the count and ID.
]]) 
end

local function r_string (resources, count, id)
    local s = ""

    for k,v in pairs (resources) do
        if resource_type == "all" or (resource_type == k and format == "all") then
            s = s..(count and "count="..#cpuset.new (resources[k]).." " or "")
            s = s..(id and k.."="..resources[k] or "")
        elseif resource_type == k then
            s = s..(count and #cpuset.new (resources[k]) or "")
            s = s..(id and resources[k] or "")
        end
    end
    return s
end

local function run (count, id)
    local key = id_to_kvs_path{ flux = f, jobid = jobid }..".R_lite"
    local R_lite = assert (f:kvs_get (key))
    local hit = false

    for _,r in pairs (R_lite) do
        local rtype = r.children[resource_type]
        if resource_type ~= "all" and not rtype then
            die ("No info in R_lite for %s\n", resource_type)
        end

        if rank == "all" then
            hit = true;
            print ("rank"..r.rank..": "..r_string (r.children, count, id))
        elseif r.rank == tonumber (rank) then
            hit = true;
            print (r_string (r.children, count, id))
        end
    end

    if hit == false then
        die ("No info in R_lite for rank %d\n", rank)
    end
end

local count = (format == "all" or format == "count")
local id = (format == "all" or format == "id")
if not jobid or #arg < 1 or (count == false and id == false) then
    usage ()
    os.exit ()
end

run (count, id)

-- vi: ts=4 sw=4 expandtab
