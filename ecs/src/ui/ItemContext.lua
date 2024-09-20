--
--
-- A compile time context for a UI item.
--
--

local ItemContext = {}

-- * --------------------------------------------------------------------------

ItemContext.new = function(schema, varname, style)
  local o = {}
  o.schema = schema
  o.varname = varname
  o.style = style
  return setmetatable(o, ItemContext)
end

-- * --------------------------------------------------------------------------

ItemContext.__index = function(self, key)
  local func = 
    assert(self.schema.funcs[key],
      "no function with name '"..key.."'")
  return function()
    return func:call(self)
  end
end

return ItemContext
