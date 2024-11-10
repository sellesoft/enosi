local lpp = require "lpp"
local Widget = require "ui.Widget"
local Schema = require "ui.Schema"
local StyleContext = require "ui.StyleContext"
local ItemContext = require "ui.ItemContext"
local Placeable = require "ui.Placeable"
local List = require "list"
local buffer = require "string.buffer"

local ui = {}

ui.widgets = {}

-- * --------------------------------------------------------------------------

ui.widget = function(name, ...)
  if ui.widgets[name] then
    error("a widget with name '"..name.."' already exists")
  end

  local widget = Widget.new(name)
  ui.widgets[name] = widget

  local schema_def = false
  local schema_def_idx = 0

  local switch =
  {
  schema = function(idx, def)
    if schema_def then
      error("a schema has already been defined for this widget")
    end
    schema_def = def
    schema_def_idx = idx
  end,
  struct = function(idx, def)
    if widget.struct then
      error("a struct has already been defined for this widget")
    end
    widget.struct = def
  end,
  inherits = function(idx, def)
    if widget.base then
      error("a base has already been defined for this widget")
    end
    if schema_def then
      error("inherits kw must come before schema def")
    end
    local base = def:match("%S+")
    local base_widget = ui.widgets[base]
    if not base_widget then
      error("base widget specified as '"..base.."' but no widget with this "..
            "name exists")
    end
    widget.base = base_widget
  end 
  }

  List{...}:eachWithIndex(function(arg,i)
    local start,stop = arg:find("^%w+")

    local listOptions = function()
      local buf = buffer.new()
      buf:put("one of:\n")
      for k in pairs(switch) do
        buf:put(" ",k,"\n")
      end
      return buf:get()
    end

    if not start then
      error("expected a keyword before widget arg\n"..listOptions())
    end

    local kw = arg:sub(start, stop)
    local def = arg:sub(stop+1)


    local case = switch[kw]
    if not case then
      error("unknown keyword '"..kw.."' expected "..listOptions())
    end

    case(i+1, def)
  end)

  if name ~= "Item" then
    local Item = ui.widgets.Item

    -- Default to Item as this widget's base if one is not specified.
    if not widget.base then
      if not Item then
        error("no base widget specified and the Item widget is not defined "..
              "in this translation unit. All widgets must have Item as "..
              "their ultimate base widget")
      end
      widget.base = Item
    end

    -- Set the base of this widget's schema.
    -- TODO(sushi) clean this up by just letting Schema know what Widget it 
    --             belongs to maybe.
    if widget.schema_def then
      widget.schema_def.base = Item.schema
    end
  end

  if schema_def then
    local base
    if widget.base then
      base = widget.base.schema_def
    end
    widget.schema_def = 
      Schema.new(
        name, 
        schema_def, 
        lpp.getMacroArgOffset(schema_def_idx),
        base)
  end

  -- Default to the base widget's schema if one is not provided.
  if not widget.schema_def then
    widget.schema_def = widget.base.schema_def
  end

  local buf = buffer.new()
  buf:put("struct ", name)
  if widget.base then
    buf:put(" : public ", widget.base.name)
  end
  buf:put("\n", widget.struct, ";")
  return buf:get()
end

-- * --------------------------------------------------------------------------

ui.createItemContext = function(widgetname, varname, maybe_null)
  local widget = 
    assert(ui.widgets[widgetname], "no widget with name "..widgetname)
  local style = 
    ui.createStyleContext(
      widget.schema_def, 
      varname.."->style", 
      varname,
      varname,
      maybe_null)
  return ItemContext.new(widget, varname, style)
end

-- * --------------------------------------------------------------------------

ui.createStyleContext = function(
    schema, 
    varname, 
    prefix, 
    itemvar, 
    itemvar_maybe_null)
  return 
    StyleContext.new(
      schema, 
      varname, 
      prefix, 
      itemvar,
      itemvar_maybe_null)
end

-- * --------------------------------------------------------------------------

ui.definePlaceable = function(name, def)
  local p = Placeable.new(name, def, lpp.getMacroArgOffset(2))
  return p:makeC()
end

return ui
