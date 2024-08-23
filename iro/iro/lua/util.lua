--- Common lua utility.

local utils = {}

--- Dumps the given value 'x'. 
---
---@param x any The value to dump.
---@param max_depth number? The max depth to print tables (default: 1)
utils.dumpValue = function(x, max_depth)
  iro__lua_inspect(x, max_depth)
end

return utils
