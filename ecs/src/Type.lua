local Type = {}

Type.make = function()
  local o = {}
  o.__index = o
  o.isTypeOf = function(x)
    return getmetatable(x) == o
  end
  return o
end

return Type
