--- 
--- Macros replacing the normal C logging macros.
---

local List = require "List"
local buffer = require "string.buffer"

log = {}

local function putLog(verb, ...)
  local args = List{...}
  if args:len() == 1 then
    if args[1]:find "^<<%-" then
      local delimiter, body_start = args[1]:match "<<%-(.-)\n()"
      if not delimiter then
        goto not_heredoc
      end
      local body, ws = 
        args[1]:match("(.*)\n(%s*)"..delimiter.."$", body_start)
      
      local buf = buffer.new()

      for line in body:gmatch "[^\n]+" do
        if line:find("^"..ws) then
          buf:put(line:sub(#ws+1), '\\n')
        end
      end

      body = buf:get()

      local result = buffer.new()
      local prev_stop = 1
      local first = true
      for start,interp,stop in body:gmatch "()%$(%b())()" do
        if first then 
          first = false
        else
          result:put ','
        end
        result:put('"', body:sub(prev_stop, start-1), '",', interp:sub(2,-2))
        prev_stop = stop
      end

      if not first then
        result:put ','
      end

      result:put('"', body:sub(prev_stop), '"')

      return verb..'('..result..')'
    end
  end

  ::not_heredoc::

  local buf = buffer.new()
  buf:put(verb, "(")
  for arg,i in args:eachWithIndex() do
    buf:put(arg)
    if i ~= args:len() then
      buf:put ","
    end
  end
  buf:put(")")

  return buf:get()
end

log.trace  = function(...) return putLog("TRACE", ...) end
log.debug  = function(...) return putLog("DEBUG", ...) end
log.info   = function(...) return putLog("INFO", ...) end
log.notice = function(...) return putLog("NOTICE", ...) end
log.warn   = function(...) return putLog("WARN", ...) end
log.error  = function(...) return putLog("ERROR", ...) end
log.fatal  = function(...) return putLog("FATAL", ...) end

log.ger = function(name, verb)
  return [[
    #include "iro/Logger.h"
    static iro::Logger logger 
      = iro::Logger::create("]]..name..[["_str, iro::Logger::Verbosity::]]..
      verb..[[);]]
end

