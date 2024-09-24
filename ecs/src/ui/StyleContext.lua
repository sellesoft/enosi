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

-- Reusable buffer for making strings. You MUST remember to use get() when 
-- returning the result of this! All functions are meant to be able to assume
-- this is clear on entry. Note that this cannot be used safely in cases like 
-- returning a function from a __index metamethod because the index will happen
-- in phase 2, but the call could be deferred until phase 3, where sbuf will 
-- have likely been cleared by some other invokation here.
-- Maybe its just better to use local buffers? I don't know, this seems nice 
-- for keeping a buffer around that will keep space to fit most strings.
local sbuf = buffer.new()

-- Reusable buffer for quick strings.
local mbuf = buffer.new()

local makeStr = function(...)
  mbuf:put(...)
  return mbuf:get()
end

local StyleContext = {}

-- * --------------------------------------------------------------------------

StyleContext.new = function(
    schema, 
    varname, 
    prefix, 
    itemvar, 
    -- If itemvar is provided, it may be null.
    itemvar_maybe_null)
  local o = {}
  o.lookups = {}
  o.schema = schema
  o.varname = varname
  -- NOTE(sushi) because prefix is optional, we have to explicitly mark it 
  --             as non-existant so that the __index metamethod is not 
  --             invoked when we try to use it.
  o.prefix = prefix or false
  o.itemvar = itemvar or false
  o.itemvar_maybe_null = itemvar_maybe_null or false
  return setmetatable(o, StyleContext)
end

-- * --------------------------------------------------------------------------

StyleContext.lookup = function(self, ...)
  -- Performing a lookup of some style properties.

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

    local get = 
      makeStr(
        self.varname,'.getAs<',property.type:getTypeName(),'>("',property.name,
        '"_hashed,',property.defval,')')

    self.lookups[propname] = lookup

    sbuf:put("auto ", name, " = ")

    if self.itemvar and self.itemvar_maybe_null then
      sbuf:put(
        "(", self.itemvar, "? ", get, " : ", property.type:getTypeName(), "(", 
        property.defval, "));\n")
    else
      sbuf:put(get, ";\n")
    end
  end)

  return sbuf:get()
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

        local buf = buffer.new()

        buf:put(
          self.varname,'.setAs<',property.type:getTypeName(),'>("',
          property.name,'"_hashed,')

        return function(val)
          buf:put(property.type:set(property, val))
          if self.itemvar then
            buf:put(",", self.itemvar)
          end
          buf:put(")")
          return buf:get()
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
