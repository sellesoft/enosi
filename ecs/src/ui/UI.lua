local lpp = require "lpp"
local Schema = require "ui.Schema"
local StyleContext = require "ui.StyleContext"
local ItemContext = require "ui.ItemContext"
local List = require "list"
local buf = require "string.buffer"

local ui = {}

ui.schemas = {}
ui.schema_defs = {}

-- * --------------------------------------------------------------------------

ui.createItemContext = function(schemaname, varname, maybe_null)
  local schema = 
    assert(ui.schema_defs[schemaname], "no schema with name "..schemaname)
  local style = 
    ui.createStyleContext(
      schemaname, 
      varname.."->style", 
      varname,
      varname,
      maybe_null)
  return ItemContext.new(schema, varname, style)
end

-- * --------------------------------------------------------------------------

ui.createStyleContext = function(
    schema, 
    varname, 
    prefix, 
    itemvar, 
    itemvar_maybe_null)
  if type(schema) == "string" then
    schema = 
      assert(ui.schema_defs[schema], "no schema with name "..schema)
  end

  return 
    StyleContext.new(
      schema, 
      varname, 
      prefix, 
      itemvar,
      itemvar_maybe_null)
end

-- * --------------------------------------------------------------------------

ui.defineSchema = function(name, def)
  local file_offset = lpp.getMacroArgOffset(2)

  -- TODO(sushi) it would be nice to pcall here to prevent showing internal
  --             errors but they're also useful for debugging so idk.
  local schema = Schema.new(name, def, file_offset)
  ui.schema_defs[name] = schema
  ui.schemas[name] = Schema.View.new(schema)
end

return ui
