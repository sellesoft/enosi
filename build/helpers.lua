local sys = require "build.sys"
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

helpers.makeRootRelativePath = function(x)
  local b, e = x:find("^"..sys.root)
  if b then
    return x:sub(e)
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

return helpers
