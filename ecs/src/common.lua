--- 
--- Common stuff throughout all of ecs.
---

-- Introduce lpp as a global.
lpp = require "Lpp"

-- Introduces the log global
require "Log"

function dbgBreak()
  return "__builtin_debugtrap()"
end

local common = {}

common.reflect = require "reflect.Reflector"
common.buffer = require "string.buffer"
common.List = require "List"

common.defFileLogger = function(name, verbosity)
  local buf = common.buffer.new()

  buf:put("static Logger logger = Logger::create(\"", name, 
          "\"_str, Logger::Verbosity::",verbosity,");")

  return buf:get()
end
-- Introduce as global given how common this is.
defFileLogger = common.defFileLogger

common.failIf = function(errval)
  return function(cond, ...)
      local args = common.buffer.new()
      local first = true
      for arg in common.List{...}:each() do
        if first then
          first = false
        else
          args:push ","
        end
        args:push(arg)
      end
      return [[
        if (]]..cond..[[)
        {
          ERROR(]]..args:get()..[[);
          return ]]..errval..[[;
        }]]
  end
end

return common
