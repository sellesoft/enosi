local List = require "list"

--- A type that provides a nice syntax for concatenating
--- strings into a List. For example, one can type:
--- ```lua
---   local t = Twine.new
---     "hello!"
---     "here"
---     "are"
---     "my"
---     "strings!"
--- ```
--- and adding more strings after creation can be done using the 
--- syntax:
--- ```lua
---   t "hello!" "here" "are" "more" "strings!"
--- ```
---@class Twine
---
--- The internal list of strings.
---@field arr List
local Twine = {}
Twine.__index = List

local assertstr = function(x)
  return assert(x, "Twine may only store strings!")
end

--- Creates a new Twine.
---
---@param init string?
---@return Twine
Twine.new = function(init)
  local o = {}
  o.arr = {}

  if init then
    assertstr(init)
    List.push(o, init)
  end

  setmetatable(o, Twine)
  return o
end

--@param x string
--@return Twine
Twine.__call = function(self, x)
  assertstr(x)
  List.push(self, x)
  return self
end

return Twine
