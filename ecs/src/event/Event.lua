local lpp = require "lpp"
local reflect = require "reflection.Reflector"
local List = require "list"
local CGen = require "cgen"
local buffer = require "string.buffer"

local log = require "logger" ("event", Verbosity.Info)

local Event = {}
Event.list = List{}
Event.map = {}

Event.events =
{
  comp = 
  {
    list = List{},
    map = {},
  },
  broad = 
  {
    list = List{},
    map = {}
  }
}

Event.comp = function(name, def)
  if Event.events.comp.map[name] then
    error("event with name '"..name.."' already defined")
  end
  Event.events.comp.map[name] = def
  Event.events.comp.list:push { name=name, def=def }

  local start, stop = def:find("%b{}")
  if not start then
    log:fatal("expected a def")
    os.exit(1)
  end

  def = def:sub(start+1,stop-1)

  local buf = buffer.new()

  buf:put("struct ", name, "\n{\n", def)
  buf:put("\n};\n")

  return buf:get()
end

Event.broad = function(name, def)
  if Event.events.broad.map[name] then
    error("event with name '"..name.."' already defined")
  end
  Event.events.broad.map[name] = def
  Event.events.broad.list:push { name=name, def=def }

  local start, stop = def:find("%b{}")
  if not start then
    log:fatal("expected a def")
    os.exit(1)
  end

  def = def:sub(start+1,stop-1)

  local buf = buffer.new()

  buf:put("struct ", name, "\n{\n", def)
  buf:put("\n};\n")

  return buf:get()
end

setmetatable(Event,
{
  __call = function(self, name, def)
    self.map[name] = def
    self.list:push{ name=name, def=def }

    local start, stop = def:find("%b{}")
    if not start then
      log:fatal("expected a def")
      os.exit(1)
    end

    def = def:sub(start+1,stop-1)

    local buf = buffer.new()

    buf:put("struct ", name, "\n{\n", def)
    buf:put("\n};\n")

    return buf:get()
  end
})

return Event
