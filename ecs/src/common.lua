--- 
--- Common stuff throughout all of ecs.
---

-- Introduce lpp as a global.
lpp = require "lpp"

local common = {}

common.reflect = require "reflect.Reflector"
common.buffer = require "string.buffer"

common.defFileLogger = function(name, verbosity)
  local buf = common.buffer.new()

  buf:put("static Logger logger = Logger::create(\"", name, 
          "\"_str, Logger::Verbosity::",verbosity,");")

  return buf:get()
end

return common
