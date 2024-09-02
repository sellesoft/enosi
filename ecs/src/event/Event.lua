local lpp = require "lpp"
local reflect = require "reflection.Reflector"
local List = require "list"
local CGen = require "cgen"

local ctx = reflect.ctx

local Event = {}
Event.events = {}

setmetatable(Event,
{
  __call = function(self)
    local beb = require "event.BroadcastEventBus"
    local sec = assert(lpp.getSectionAfterMacro(),
      "attempt to create an Event but no section follows.")

    -- Insert missing 'struct'
    sec:insertString(0, "struct ")

    ctx:loadString(sec:getString())

    local result = ctx:parseTopLevelDecl()

    local decl = assert(result.decl,
      "failed to parse event")

    local declname = decl:name()
        
    print("registering event "..declname)

    self.events[declname] = decl
  end
})


return Event
