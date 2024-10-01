--
--
-- A compile time context for a UI item.
--
--

local ItemContext = {}

-- * --------------------------------------------------------------------------

ItemContext.new = function(widget, varname, style)
  local o = {}
  o.widget = widget
  o.varname = varname
  o.style = style
  return setmetatable(o, ItemContext)
end

-- * --------------------------------------------------------------------------

ItemContext.__index = function(self, key)
  local func = 
    assert(self.widget.schema_def.funcs[key],
      "no function with name '"..key.."'")
  return function()
    return func:call(self)
  end
end

return ItemContext
