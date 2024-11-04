local util = require "util"

local Type = {}
Type.__index = Type

Type.make = function()
  local o = {}
  o.__index = o
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

Type.derive = function(self)
  local o = setmetatable({}, { __index = self })
  o.__index = o
  return o
end

return Type
