--
--
-- List type. Primarily cause I don't like using lua's raw tables for arrays.
-- 
-- Any function that modifies the list and doesnt already return a value will
-- return self to facilitate function chaining.
--
-- TODO(sushi) this could be optimized by making it so that its backend is secretly C.
--             or maybe it wouldn't make much of a difference, but it should be 
--             attempted eventually.
--
--

--- A wrapper around Lua's table mostly just cause I don't like using 
--- Lua's table as an array.
---
---@generic T
---@class List
---@field arr `T`[]
local List = {}

List.__index = function(self, n)
	if type(n) == "number" then
		return rawget(self, "arr")[n]
	end
	return List[n]
end

setmetatable(List, List)

local isList = function(x) return getmetatable(x) == List end

--- Construct a new List, optionally passing an initial value.
--- If 'init' is another List, the new list will be a shallow copy of it.
--- If 'init' is a Lua table, this List will simply take it.
--- Otherwise the list will be returned empty.
---
---@generic T
---@param init table | List<T>
List.new = function(init)
	if init then
		assert(type(init) == "table" or isList(init))
	else
		init = {}
	end

	local o = {}
	setmetatable(o, List)

	if isList(init) then
		for elem in init:each() do
			o:push(elem)
		end
	else
		o.arr = init
	end

	return o
end

List.__call = function(self, init)
	return List.new(init)
end

--- Returns the length of this List.
---@return number
List.len = function(self)
	return #self.arr
end

--- Pushes the given element.
---@params elem T
List.push = function(self, elem)
	table.insert(self.arr, elem)
	return self
end

List.pop = function(self)
	return table.remove(self.arr)
end

List.insert = function(self, idx, elem)
	if not elem then
		table.insert(self.arr, idx)
	else
		table.insert(self.arr, idx, elem)
	end
	return self
end

List.remove = function(self, idx)
	return table.remove(self.arr, idx)
end

--- Returns an iterator function that gives each element of 
--- this List.
---
---@return function
List.each = function(self)
	local i = 0
	return function()
		i = i + 1
		if i > self:len() then
			return nil
		end
		return self[i]
	end
end

--- The same as each, but returns as a second value the
--- index of the element.
---
---@return function
List.eachWithIndex = function(self)
	local i = 0
	return function()
		i = i + 1
		if i > self:len() then
			return nil
		end
		return self[i], i
	end
end

List.__concat = function(self, rhs)
	assert(type(rhs) == "table" or isList(rhs), "List can only concat with tables or other Lists!")

	local out = List(self)

	for _,elem in rhs.arr or rhs do
		out:push(elem)
	end

	return out
end

return List
