local cmn = require "common"
local Type = require "Type"

local Schema = Type.make()
Schema.schemas = {}

Schema.def = function(name, def)
  if Schema.schemas[name] then
    error("schema with name "..name.." already exists")
  end

  Schema.schemas[name] = true

  return "struct "..name..def..";"
end

return Schema
