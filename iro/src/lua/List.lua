--
-- List type. Primarily cause I don't like using lua's raw tables for arrays.
-- 
-- Any function that modifies the list and doesnt already return a value will
-- return self to facilitate function chaining.
--

local Type = require "Type"

---@class iro.List : iro.Type
local List = Type.make()

--- Construct a new List, optionally passing an initial value.
--- If 'init' is another List, the new list will be a shallow copy of it.
--- If 'init' is a Lua table, this List will simply take it.
--- Otherwise the list will be returned empty.
---
---@param init table | iro.List
---@return iro.List
List.new = function(init)
  if init then
    assert(type(init) == "table", 
      "List.new got a "..type(init).." instead of a table")
  else
    init = {}
  end

  if List:isTypeOf(init) then
    local o = setmetatable({}, List)
    for elem in init:each() do
      o:push(elem)
    end
    return o
  else
    return setmetatable(init, List)
  end
end

--- Returns the length of this List.
---@return number
List.len = function(self)
  return #self
end

--- Test if this list is empty
---@return boolean
List.isEmpty = function(self)
  return 0 == self:len()
end

--- Retrieves the last element of this List.
List.last = function(self)
  return self[#self]
end

--- Pushes the given element.
---@params elem T
List.push = function(self, elem)
  table.insert(self, elem)
  return self
end

List.pop = function(self)
  return table.remove(self)
end

List.pushFront = function(self, elem)
  table.insert(self, 1, elem)
  return self
end

List.popFront = function(self)
  return table.remove(self, 1)
end

List.pushList = function(self, l)
  for elem in l:each() do self:push(elem) end
  return self
end

List.insert = function(self, idx, elem)
  if not elem then
    table.insert(self, idx)
  else
    table.insert(self, idx, elem)
  end
  return self
end

List.remove = function(self, idx)
  return table.remove(self, idx)
end

--- Returns an iterator function that gives each element of 
--- this List or, if 'f' is provided, calls f with each 
--- element of the list and returns self
---
---@param f function?
---@return function | self
List.each = function(self, f)
  local i = 0
  local iter = function()
    i = i + 1
    if i > self:len() then
      return nil
    end
    return self[i]
  end

  if f then
    for e in iter do
      f(e)
    end
    return self
  else
    return iter
  end
end

--- Returns an iterator function that gives each element of 
--- this List or, if 'f' is provided, calls f with each 
--- element of the list and returns self
---
---@param f function?
---@return function | self
List.eachReverse = function(self, f)
  local i = self:len()+1
  local iter = function()
    i = i - 1
    if i == 0 then
      return nil
    end
    return self[i]
  end

  if f then
    for e in iter do
      f(e)
    end
    return self
  else
    return iter
  end
end

--- Returns the first element for which the return value of 
--- f is truthy.
List.find = function(self, f)
  for e in self:each() do
    if f(e) then
      return e
    end
  end
end

--- Returns the index of the element for which the return value of
--- f is truthy.
List.index = function(self, f)
  for e,i in self:eachWithIndex() do
    if f(e) then
      return i
    end
  end
end

--- Returns the return values of f for the first element in which
--- the first return value of f is truthy
List.findAndMap = function(self, f)
  for e in self:each() do
    -- There's gotta be a way to do this w/o capturing results in a table.
    local result = {f(e)}
    if result[1] then
      return unpack(result)
    end
  end
end

--- The same as each, but returns as a second value the
--- index of the element.
---
---@param f function?
---@return function | self
List.eachWithIndex = function(self, f)
  local i = 0
  local iter = function()
    i = i + 1
    if i > self:len() then
      return nil
    end
    return self[i], i
  end

  if f then
    for e,i in iter do
      f(e, i)
    end
    return self
  else
    return iter
  end
end

--- Skips elements until the callback returns true and returns the element 
--- arrived. If the element is not found we return nil or the optional not
--- found value.
---@param f function
---@param not_found_value any?
List.skipUntil = function(self, f, not_found_value)
  for e in self:each() do
    if f(e) then
      return e
    end
  end
  return not_found_value
end

--- Apply function 'f' to each element of this List and return
--- a new List containing the results.
---
--- TODO(sushi) implement lazy iterators and replace this 
---
---@param f function
---@return iro.List
List.map = function(self, f)
  local new = List{}
  for e in self:each() do
    new:push(f(e))
  end
  return new
end

List.sub = function(self, i, j)
  return List{unpack(self, i, j)}
end

--- Returns a new List with all elements from nested Lists flattened into
--- it.
-- TODO(sushi) it would be nice to implement a lazy iterator type eventually.
---@return List
List.flatten = function(self)
  local o = List{}

  local function recur(l)
    for elem in l:each() do
      if List == Type.of(elem) then
        recur(elem)
      else
        o:push(elem)
      end
    end
  end

  recur(self)

  return o
end

return List
