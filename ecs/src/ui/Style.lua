---
---
--- Definitions for the Style map.
---
---

local List = require "List"
local buffer = require "string.buffer"
local Parser = require "Parser"

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

--- Generates a table containing style property keys and their values
--- from a style string.
Style.parse = function(widget, text)
  local style = {}
  local schema = widget.schema_def
  local parser = Parser.new(text,0)

  -- Consume possible vertical bar.
  parser:checkToken "|"

  while true do
    local propname = parser:expectIdentifier()

    local property = schema:findProperty(propname)

    if not property then
      parser:errorHere("no property named '",propname,"' in schema of ",
                       widget.name)
    end

    if property.base_property then
      property = property.base_property
    end

    parser:expectToken ":"

    if parser:checkToken "inherit" then
      style[propname] = "inherit"
    elseif parser:checkToken "%$" then
      local exp = 
        parser:expectPattern("%b()", 
        "parenthesized expression for interpolation")

      style[propname] = {property=property, value=exp:sub(2,-2)}
    else
      local result
      if not property.valueParser then
        if not property.type.valueParser then
          parser:errorHere("property ", property.name, " does not define ",
                           "a value parser nor does its underlying type")
        end
        
        result = property.type:valueParser(property, parser)
      else
        -- TODO(sushi) loading the lua chunks for parsing these values 
        --             should only be done once per property.
        local vparser, err = loadstring(
          "return function(parser, style)\n"..property.valueParser..
          "\nend",
          property.name..":valueParser()")

        if vparser == nil then
          parser:errorHere("failed to load valueParser for property "..
                           property.name, ":\n", err)
        end

        local interface = 
        {
          set = setmetatable({},
          {
            __index = function(_, key)
              local prop = schema:findProperty(key)
              if not prop then
                parser:errorHere("no property named ",key," in schema ",
                                 "of widget ",widget.name)
              end
              return function(val)
                style[key] = 
                {
                  property=prop,
                  value=prop.type:set(prop,val)
                }
              end
            end,
          })
        }

        local res, err = pcall(vparser(), parser, interface)

        if not res then
          error(err)
        end

        result = err
      end

      style[propname] = {property=property, value=result}
    end
    if not parser:checkToken "|" then
      break
    end
  end

  return style
end

return Style
