--
--
-- List of strings that can be initialized and concatenated in
-- a convenient way, eg.
--
--   local t = Twine "here" "are" "my" "strings!"
--
--

local List = require "list"

local Twine = {}
Twine.__index = Twine

local assertstr = function(x)
	return assert(x, "Twine may only store strings!")
end

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
