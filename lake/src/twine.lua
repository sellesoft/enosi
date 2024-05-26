--
--
-- List of strings that can be initialized and concatenated in
-- a convenient way, eg.
--
--   local t = Twine "here" "are" "my" "strings!"
--
--

local List = require "list"

--- A list of strings that provides a nice syntax for concatenating
--- strings. For example, one can type:
--- ```lua
---		local t = Twine.new
---			"hello!"
---			"here"
---			"are"
---			"my"
---			"strings!"
--- ```
--- and adding more strings after creation can be done using the 
--- syntax:
--- ```lua
--- 	t "hello!" "here" "are" "more" "strings!"
--- ```
---@class Twine
---
--- The internal list of strings.
---@field arr List<string>
local Twine = {}
Twine.__index = Twine

local assertstr = function(x)
	return assert(x, "Twine may only store strings!")
end

--- Creates a new Twine.
---
---@param init string?
---@return Twine
Twine.new = function(init)
	local o = {}
	setmetatable(o, Twine)
	o.arr = List()

	if init then
		assertstr(init)
		o.arr:push(init)
	end

	return o
end

Twine.__call = function(self, x)
	assertstr(x)
	self.arr:push(x)
	return self
end

Twine.each = function(self)
	return self.arr:each()
end

return Twine
