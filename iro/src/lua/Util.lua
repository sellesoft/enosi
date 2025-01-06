--- Common lua utility.

local utils = {}

--- Dumps the given value 'x'. 
---
---@param x any The value to dump.
---@param max_depth number? The max depth to print tables (default: 1)
utils.dumpValue = function(x, max_depth)
  iro__lua_inspect(x, max_depth)
end

utils.deepCopy = function(t)
  local o = {}
  for k,v in pairs(t) do
    if type(v) == "table" then
      o[k] = utils.deepCopy(v)
    else
      o[k] = v
    end
  end
  return setmetatable(o, getmetatable(t))
end

return utils
