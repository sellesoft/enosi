---
---
--- Definitions for the Style map.
---
---

local List = require "list"

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

pushPropKind("u32", "enum_value", "Enum")
pushPropKind("u64", "flag_value", "Flags")
pushPropKind("Color", "color", "Color")
pushPropKind("vec2f", "v2", "Vec2f")
pushPropKind("vec4f", "v4", "Vec4f")

return Style
