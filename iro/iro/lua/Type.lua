local util = require "util"

---@class iro.Type
local Type = {}
Type.__index = Type

---@return iro.Type
Type.make = function()
  local o = {}
  o.__index = o
  o.__call = Type.__call
  o.__type = o
  return setmetatable(o, Type)
end

Type.isTypeOf = function(self, x)
  local mt = getmetatable(x)
  while mt do
    local i = rawget(mt, "__index")
    if rawequal(self, i) then
      return true
    end
    mt = getmetatable(i)
  end
  return false
end

Type.is = function(self, T)
  if rawequal(self,T) then
    return true
  end
  local Self = getmetatable(self)
  while Self do
    local i = rawget(Self, "__index")
    if rawequal(T, i) then
      return true
    end
    Self = getmetatable(i)
  end
  return false
end

Type.__call = function(self, ...)
  return self.new(...)
end

---@return any
Type.derive = function(self)
  local o = setmetatable({}, 
  { 
    __index = self,
    __call = self.__call
  })
  o.__index = o
  o.__tostring = self.__tostring
  o.__type = o
  return o
end

return Type
