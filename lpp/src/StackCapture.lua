local List = require "List"
local lpp = require "Lpp"

---@class Call
--- The identifier of the source file this call was made in.
---@field src string
--- The name of the function being called, if available.
---@field name string?
--- The line number in src where this call was made
---@field line number 

---@return List
return function(offset)
  local stack = List()
  offset = offset or 0
  offset = offset + 2

  local fidx = offset
  while true do
    local info = debug.getinfo(fidx)
    if not info then break end
    if info.what ~= "C" then
      if not lpp.stacktrace_func_filter[info.func] then
        stack:push
        {
          src = info.source,
          line = info.currentline,
          name = lpp.stacktrace_func_rename[info.func] or info.name,
          metaenv = getfenv(info.func).__metaenv
        }
      end
    end
    fidx = fidx + 1
  end

  return stack
end
