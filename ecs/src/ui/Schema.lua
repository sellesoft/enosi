---
---
--- Schemas providing the properties valid for a ui item/widgets style.
---
---

local List = require "list"
local util = require "util"
local CGen = require "cgen"
local lpp = require "lpp"

local Schema,
      Property,
      Enum,
      Flags,
      CType

local makeType = function()
  local o = {}
  o.__index = o
  o.isTypeOf = function(x)
    return getmetatable(x) == o
  end
  return o
end

-- * ==========================================================================
-- *   Schema
-- * ==========================================================================

Schema = makeType()

-- * --------------------------------------------------------------------------

Schema.new = function(name, def, file_offset)
  local o = setmetatable({}, Schema)
  o.name = name
  o.member = {}
  o.member.list = List{}
  o.member.map = {}
  o:parse(def, file_offset)
  return o
end

-- * --------------------------------------------------------------------------

Schema.findMember = function(self, name)
  return self.member.map[name]
end

-- * --------------------------------------------------------------------------

--- Parses 'def' into this schema.
Schema.parse = function(self, def, file_offset)
  local offset = 1

  local errorHere = function(msg)
    local line, column = 
      lpp.getLineAndColumnFromOffset(file_offset + offset - 1)
    error("\nat "..line..":"..column..": "..msg, 2)
  end

  local skipWhitespace = function()
    local found,stop = def:find("^%s*", offset)
    if found then
      offset = stop+1
    end
  end

  local expectToken = function(token)
    skipWhitespace()
    local start, stop = def:find("^"..token, offset)
    if not start then
      errorHere("expected '"..token.."'")
    end
    offset = stop+1
  end

  local expectIdentifier = function()
    skipWhitespace()
    local start, stop = def:find("^[%w_-]+", offset)
    if not start then
      errorHere("expected an identifier")
    end
    local id = def:sub(start, stop)
    offset = stop+1
    return id
  end

  local expectPattern = function(pattern, expected)
    skipWhitespace()
    local start, stop = def:find("^"..pattern, offset)
    if not start then
      errorHere("expected "..expected)
    end
    offset = stop+1
    return def:sub(start, stop)
  end

  local checkToken = function(token)
    skipWhitespace()
    local start, stop = def:find("^"..token, offset)
    if start then
      offset = stop+1
      return true
    end
  end

  local checkPattern = function(pattern)
    skipWhitespace()
    local start, stop = def:find("^"..pattern, offset)
    if start then
      offset = stop+1
      return true
    end
  end

  local parseEnumOrFlags = function()
    local elems = List{}
    expectToken "{"
    while true do
      elems:push(expectIdentifier())
      
      if not checkToken "," then
        expectToken "}"
        return elems
      else
        if checkToken "}" then
          return elems
        end
      end
    end
  end

  local parseProperty = function(is_alias)
    local id = expectIdentifier()
    expectToken ":"
   
    local typename = expectIdentifier()

    local handlers =
    {
      enum = function()
        local enum = Enum.new(parseEnumOrFlags())
        return enum
      end,
      flags = function()
        local flags = Flags.new(parseEnumOrFlags())
        return flags
      end,
    }

    local type
    local handler = handlers[typename]
    if handler then
      type = handler()
    else
      -- assume this is some C++ type.
      type = CType.new(typename)
    end

    local defval

    -- requiring a default value for now.
    expectToken "="

    if typename == "enum" or
       typename == "flags" then
      local elem = expectIdentifier()
      if not type.elems.map[elem] then
        errorHere(
          "default value specified as '"..elem.."' but this is not "..
          "defined")
      end
      defval = elem
    else
      if is_alias then
        defval = expectPattern('".-"', "string for alias")
        defval = defval:sub(2,-2)
        if not defval:find("%%", 0) then
          errorHere("alias string must contain % to replace with style lookup")
        end
      else
        defval = 
          expectPattern("%b{}", "a braced initializer list (eg. {...})")
      end
    end

    return id, type, defval
  end

  expectToken "{"
  while true do
    local id, type, defval = parseProperty(false)

    local property = Property.new(id, type, defval, nil, self, errorHere)

    local need_semicolon = true

    if checkPattern "alias" then
      while true do
        local id, type, defval = parseProperty(true)

        Property.new(id, type, defval, property, self, errorHere)

        if not checkToken "," then
          if checkToken ";" then
            need_semicolon = false
            break
          else
            errorHere("expected a ';' or ',' after alias definition")  
          end
        end
      end
    end

    if need_semicolon then
      expectToken ";"
    end

    if checkToken "}" then
      break
    end
  end

  -- util.dumpValue(self, 8)
end

-- * ==========================================================================
-- *   Property
-- * ==========================================================================

Property = makeType()
Schema.Property = Property

-- * --------------------------------------------------------------------------

Property.new = function(name, type, defval, aliased_property, schema, errcb)
  local o = {}
  o.name = name
  o.type = type
  o.defval = defval
  o.aliased_property = aliased_property
  if schema.member.map[name] then
    if aliased_property then
      errcb(
        "cannot alias as '"..name.."' as a property with this name already "..
        "exists")
    else
      errcb("a property with name '"..name.."' already exists")
    end
  end
  schema.member.map[name] = o
  schema.member.list:push(o)
  return setmetatable(o, Property)
end

-- * ==========================================================================
-- *   Enum
-- * ==========================================================================

Enum = makeType()
Schema.Enum = Enum

-- * --------------------------------------------------------------------------

Enum.new = function(elems)
  local o = {}
  o.elems = {}
  o.elems.list = elems
  o.elems.map = {}
  o.elems.list:each(function(elem)
    o.elems.map[elem] = true
  end)
  return setmetatable(o, Enum)
end

-- * ==========================================================================
-- *   Flags
-- * ==========================================================================

Flags = makeType()
Schema.Flags = Flags

-- * --------------------------------------------------------------------------

Flags.new = function(elems)
  local o = {}
  o.elems = {}
  o.elems.list = elems
  o.elems.map = {}
  o.elems.list:each(function(elem)
    o.elems.map[elem] = true
  end)
  return setmetatable(o, Flags)
end

-- * ==========================================================================
-- *   Type
-- * ==========================================================================

CType = makeType()
Schema.CType = CType

-- * --------------------------------------------------------------------------

CType.new = function(name)
  local o = {}
  o.name = name
  return setmetatable(o, CType)
end

-- * --------------------------------------------------------------------------

CType.getUnionName = function(self)
  return "_"..self.name
end

return Schema
