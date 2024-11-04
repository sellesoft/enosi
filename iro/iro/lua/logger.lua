--- 
--- Lua interface to iro's log.
---

local ffi = require "ffi"
ffi.cdef
[[
  typedef enum
  {
    Trace,
    Debug,
    Info,
    Notice,
    Warn,
    Error,
    Fatal,
  } Verbosity;

  u64 iro_loggerSize();
  void iro_initLogger(void* logger, String name, u32 verbosity);
  b8 iro_logFirst(void* logger, u32 verbosity, String s);
  void iro_logTrail(void* logger, u32 verbosity, String s);
  void iro_loggerSetName(void* logger, String name);

]]
local C = ffi.C

Verbosity =
{
  Trace  = C.Trace,
  Debug  = C.Debug,
  Info   = C.Info,
  Notice = C.Notice,
  Warn   = C.Warn,
  Error  = C.Error,
  Fatal  = C.Fatal,
}

local str_type = ffi.typeof("String")

local makeStr = function(s)
  return str_type(s, #s)
end

local logger_size = tonumber(C.iro_loggerSize())
local logger_type = ffi.typeof("u8["..logger_size.."]")

---@class Logger
local Logger = {}
Logger.__index = Logger

local doLog = function(logger, verbosity, first, ...)
  if 0 == C.iro_logFirst(logger.handle, verbosity, makeStr(tostring(first))) then
    return
  end

  for _,arg in pairs{...} do
    C.iro_logTrail(logger.handle, verbosity, makeStr(tostring(arg)))
  end
end

Logger.trace  = function(self, ...) doLog(self, Verbosity.Trace,  ...) end
Logger.debug  = function(self, ...) doLog(self, Verbosity.Debug,  ...) end
Logger.info   = function(self, ...) doLog(self, Verbosity.Info,   ...) end
Logger.notice = function(self, ...) doLog(self, Verbosity.Notice, ...) end
Logger.warn   = function(self, ...) doLog(self, Verbosity.Warn,   ...) end
Logger.error  = function(self, ...) doLog(self, Verbosity.Error,  ...) end
Logger.fatal  = function(self, ...) doLog(self, Verbosity.Fatal,  ...) end

Logger.setName = function(self, name)
  C.iro_loggerSetName(self.handle, name)
end

local createLogger = function(name, verbosity)
  local logger = ffi.new(logger_type)

  local name = ffi.new("String", name, #name)

  C.iro_initLogger(logger, name, verbosity)

  return setmetatable(
  {
    handle = logger,
    name = name
  }, Logger)
end

return createLogger
