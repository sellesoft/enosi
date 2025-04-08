local cmn = require "common"
local List = require "List"
local buffer = require "string.buffer"
local Type = require "Type"
local metadata = require "reflect.Metadata"

local log = require "Logger" ("event", Verbosity.Info)

local Event = {}
Event.list = List{}
Event.map = {}

local EventTrait = Type.make()

EventTrait.new = function(name)
  local o = {}
  o.name = name
  return setmetatable(o, EventTrait)
end

EventTrait.attach = function(self)
  return metadata._has_event_trait(self.name)
end


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
