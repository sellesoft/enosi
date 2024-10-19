local lpp = require "lpp"
local reflect = require "reflection.Reflector"
local List = require "list"
local CGen = require "cgen"

local ctx = reflect.ctx

local Event = {}
Event.list = List{}
Event.map = {}

setmetatable(Event,
{
  __call = function(self)
    local sec = assert(lpp.getSectionAfterMacro(),
      "attempt to create an Event but no section follows.")

    -- Insert missing 'struct'
    sec:insertString(0, "struct ")

    ctx:loadString(sec:getString())

    -- Why the hell am I using clang to parse this?
    local result = ctx:parseTopLevelDecl()

    local decl = assert(result.decl,
      "failed to parse event")

    local declname = decl:name()
        
    -- print("registering event "..declname)

    self.map[declname] = decl
    self.list:push{ name=declname, decl=decl }
  end
})

return Event
