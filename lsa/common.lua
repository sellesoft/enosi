--- 
--- Common stuff.
---

local co = require "coroutine"
local errh = require "errh"

local common = {}

common.Type = require "Type"
common.List = require "list"
common.errh = errh
common.co = co

log = require "logger" ("lsa", Verbosity.Info)

fatal = function(...)
  log:fatal(debug.traceback(), "\n", ...)
  log:fatal "\n"
  os.exit(1)
end

common.pco = function(f)
  assert(f)
  return co.create(function(...)
    local result = 
      {xpcall(function(...)
        return f(...)
      end, errh, ...)}
    if not result[1] then
      errh(result[2])
    else
      return unpack(result, 2)
    end
  end)
end

return common
