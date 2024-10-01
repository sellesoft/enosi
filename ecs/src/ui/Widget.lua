---
---
--- Underlying information about a widget in the ui system, eg. its Schema
--- and C++ structure definition.
---
---

local Type = require "Type"
local Schema = require "ui.Schema"

-- * ==========================================================================
-- *   Widget
-- * ==========================================================================

--- A specialization of a ui Item.
---@class ui.Widget
--- The name of this widget.
---@field name string
--- The definition of this widget's Schema.
---@field schema_def ui.Schema
--- Definition of this Widget's internal structure.
---@field struct string?
--- The base of this Widget.
---@field base ui.Widget?
local Widget = Type.make()

-- * --------------------------------------------------------------------------

Widget.new = function(name)
  local o = {}
  o.name = name
  o.schema_def = false
  o.struct = false
  o.base = false
  return setmetatable(o, Widget)
end

-- * --------------------------------------------------------------------------

Widget.__index = function(self, key)
  if key == "schema" then
    return Schema.View.new(self.schema_def)
  end
end

return Widget
