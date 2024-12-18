--- 
--- Various helpers to be used in the build system.
---
--- The build system should NOT be used here. Any info needed from it should
--- be passed to the function.
---

local Type = require "Type"
local List = require "list"

local helpers = {}

helpers.switch = function(x, cases, default)
  if not x then return default end
  local case = cases[x]
  if case then
    return case
  else
    return default
  end
end

helpers.listBuilder = function(...)
  local out = List{}
  for arg in List{...}:each() do
    if arg then
      if type(arg) ~= "string" or #arg ~= 0 then
        out:push(arg)
      end
    end
  end
  return out
end

helpers.makeRootRelativePath = function(root, x)
  local b, e = x:find("^"..root)
  if b then
    return x:sub(e+2)
  end
  return x
end

helpers.getPathBasename = function(x)
  local b, e = x:find("/(.*)$")
  if b then 
    return x:sub(e)
  end
  return x
end

helpers.getPathDirname = function(x)
  local b,e, dirname = x:find("(.*)/.*$")
  if b then
    return dirname
  end
  return x
end

helpers.indexer = function(f)
  return setmetatable({}, { __index = f })
end

helpers.caller = function(f)
  return setmetatable({}, { __call = f })
end

helpers.callindexer = function(fc, fi)
  return setmetatable({}, { __index=fi, __call=fc })
end

local EnumElem = Type.make()

EnumElem.__tostring = function(self)
  return self.name
end

helpers.enum = function(...)
  local o = Type.make()

  for elem in List{...}:each() do
    o[elem] = setmetatable({name=elem}, EnumElem)
  end

  return o
end

return helpers
