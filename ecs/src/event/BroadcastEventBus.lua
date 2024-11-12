local lpp = require "lpp"
local reflect = require "reflect.Reflector"
local event = require "event.Event"
local List = require "list"
local CGen = require "cgen"

local BroadcastEventBus = {}

BroadcastEventBus.gen = function()
  local c = CGen.new()
  for e,d in pairs(event.events) do
    c:appendStructMember("Array<"..e..">", e.."_array")
    c:append("void subscribeTo",e,"(",e,"& event);")
  end
  return c.buffer:get()
end

BroadcastEventBus.getRaiseInternalFuncSig = function(event_name)
  return 
    "extern void __BroadcastEventBus_raise"..event_name..
    "(const BroadcastEventBus*,"..event_name.."&)"
end

BroadcastEventBus.getRaiseInternalCall = function(event_name)
  return 
    "__BroadcastEventBus_raise"..event_name.."(this, event);"
end

return BroadcastEventBus
