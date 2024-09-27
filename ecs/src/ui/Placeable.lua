--- 
--- A 'placeable' set of UI widgets and such.
---

local Type = require "Type"

---@class Placeable
local Placeable = Type.make()

Placeable.new = function(name, def, file_offset)
  local o = setmetatable({}, Placeable)
  o.name = name
  o:parse(def, file_offset)
end

Placeable.parse = function(self, def, file_offset)


end
