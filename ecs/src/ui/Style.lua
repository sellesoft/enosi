---
---
--- Definitions for the Style map.
---
---

local List = require "list"
local buffer = require "string.buffer"

local Style = {}

Style.prop_kinds = List{}

local pushPropKind = function(typename, unionname, kindname)
  Style.prop_kinds:push
  {
    typename=typename,
    unionname=unionname,
    kindname=kindname,
  }
end

-- These result in the generation of setAs and getAs functions in Style.lpp
pushPropKind("u32", "enum_value", "Enum")
pushPropKind("u64", "flag_value", "Flags")
pushPropKind("Color", "color", "Color")
pushPropKind("vec2f", "v2", "Vec2f")
pushPropKind("vec4f", "v4", "Vec4f")
pushPropKind("f32", "_f32", "Float")
pushPropKind("String", "string", "String")

--- Helpers for forming the code to set a property on a style map.

Style.setAs = function(var, property, val, itemvar, eval_val)
  local buf = buffer.new()

  buf:put(
    var,'.setAs<',property.type:getTypeName(),'>("',property.name,'"_hashed,')

  if eval_val then
    buf:put(property.type:set(property, val))
  else
    buf:put(val)
  end

  if itemvar then
    buf:put(",",itemvar)
  end

  buf:put(")")

  return buf:get()
end

Style.setAsInherited = function(var, property, itemvar)
  local buf = buffer.new()

  buf:put(
    var,'.setAsInherited<',property.type:getTypeName(),'>("',property.name,
    '"_hashed')

  if itemvar then
    buf:put(",",itemvar)
  end

  buf:put(")")
  return buf:get()
end

return Style
