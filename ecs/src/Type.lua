local Type = {}

Type.make = function()
  local o = {}
  o.__index = o
  o.isTypeOf = function(x)
    return getmetatable(x) == o
  end
end

return Type
