--
--
-- A compile time representation of some item's style.
--
-- Provides ways to perform cached lookups on a Schema's properties
-- and then refer to those lookups later.
--  
--

local lpp = require "lpp"
local buffer = require "string.buffer"
local log = require "logger" ("ui.StyleContext", Verbosity.Notice)
local List = require "list"
local Schema = require "ui.Schema"

local StyleContext = {}

-- * --------------------------------------------------------------------------

StyleContext.new = function(schema, varname, prefix, item_var_if_null)
  local o = {}
  o.lookups = {}
  o.varname = varname
  -- NOTE(sushi) because prefix is optional, we have to explicitly mark it 
  --             as non-existant so that the __index metamethod is not 
  --             invoked when we try to use it.
  o.prefix = prefix or false
  o.schema = schema
  o.item_var_if_null = item_var_if_null or false
  return setmetatable(o, StyleContext)
end

-- * --------------------------------------------------------------------------

StyleContext.lookup = function(self, ...)
  -- Performing a lookup of some style properties.

  local buf = buffer.new()

  local getLookup = function(property)
    local name = property.name
    local defval = property.defval

    local switch =
    {
    [Schema.Enum] = "u32",
    [Schema.Flags] = "u64",
    [Schema.CType] = property.type.name
    }

    local typename = 
      assert(switch[getmetatable(property.type)],
        "unhandled property type")

    local lookup = 
      self.varname..".getAs<"..typename..">(\""..
      name.."\"_str, "..defval..")"

    return lookup
  end

  List{...}:each(function(propname)
    local property = 
      assert(self.schema:findMember(propname), 
        "no property '"..propname.."' in schema '"..self.schema.name.."'")

    local name = propname
    if self.prefix then
      name = self.prefix.."_"..name
    end

    local lookup = 
      Schema.Lookup.new(property, name)

    self.lookups[propname] = lookup

    if self.item_var_if_null then
      buf:put("auto ", name, " = (", 
        self.item_var_if_null, "? ", getLookup(property), " : ", 
        property.type:getTypeName(), "(", property.defval, "));\n")
    else
      buf:put("auto ", name, " = ", getLookup(property), ";\n")
    end

  end)

  return buf:get()
end

-- * --------------------------------------------------------------------------

StyleContext.__index = function(self, key)
  local raw = StyleContext[key]
  if raw then
    return raw
  end

  if key == "set" then
    return setmetatable({},
    {
      __index = function(_, key)
        local property = 
          assert(self.schema:findMember(key), 
            "no property '"..key.."' in schema '"..self.schema.name.."'")

        return function(val)
          return self.varname.."."..property.type:set(property, val)
        end
      end
    })
  end

  -- Accessing a looked up property.
  local lookup = 
    assert(self.lookups[key],
      "property '"..key.."' has not been looked up yet")
  return lookup
end

-- * --------------------------------------------------------------------------

StyleContext.__call = function(self)
  error("attempt to call a StyleContext")
end


return StyleContext
