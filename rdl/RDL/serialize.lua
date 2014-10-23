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

local szt = {}

local function char(c) return ("\\%3d"):format(c:byte()) end
local function szstr(s) return ('"%s"'):format(s:gsub("[^ !#-~]", char)) end
local function szfun(f) return "loadstring"..szstr(string.dump(f)) end
local function szany(...) return szt[type(...)](...) end

local function sztbl(t,code,var)
  for k,v in pairs(t) do
    local ks = szany(k,code,var)
    local vs = szany(v,code,var)
    code[#code+1] = ("%s[%s]=%s"):format(var[t],ks,vs)
  end
  return "{}"
end

local function memo(sz)
  return function(d,code,var)
    if var[d] == nil then
      var[1] = var[1] + 1
      var[d] = ("_[%d]"):format(var[1])
      local index = #code+1
      code[index] = "" -- reserve place during recursion
      code[index] = ("%s=%s"):format(var[d],sz(d,code,var))
    end
    return var[d]
  end
end

szt["nil"]      = tostring
szt["boolean"]  = tostring
szt["number"]   = tostring
szt["string"]   = szstr
szt["function"] = memo(szfun)
szt["table"]    = memo(sztbl)

function serialize(d)
  local code = { "local _ = {}" }
  local value = szany(d,code,{0})
  code[#code+1] = "return "..value
  if #code == 2 then return code[2]
  else return table.concat(code, "\n")
  end
end

return serialize
