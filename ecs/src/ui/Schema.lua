---
---
--- Schemas providing the properties valid for a ui item/widgets style.
---
---

local List = require "list"
local util = require "util"
local CGen = require "cgen"
local lpp = require "lpp"
local buffer = require "string.buffer"

local Schema,
      View,
      Property,
      Lookup,
      Enum,
      Flags,
      CType,
      Func

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
  o.property = {}
  o.property.list = List{}
  o.property.map = {}
  o.funcs = {}
  o:parse(def, file_offset)
  return o
end

-- * --------------------------------------------------------------------------

Schema.findMember = function(self, name)
  return self.property.map[name]
end

-- * --------------------------------------------------------------------------

Schema.__index = function(self, key)
  local raw = Schema[key]
  if raw then
    return raw
  end

  return assert(self.property.map[key],
    "no property named '"..key.."' in schema '"..self.name.."'")
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

  local parseProperty = function()
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
      defval = type.elems.map[elem]
    else
      defval = 
        expectPattern("%b{}", "a braced initializer list (eg. {...})")
    end

    return id, type, defval
  end

  expectToken "{"
  while true do
    if checkToken "func" then
      local id = expectIdentifier()
      local def = expectPattern("%b{}", "braced function definition")
      def = def:sub(2,-2)
      Func.new(id, def, self, errorHere)
    else
      local id, type, defval = parseProperty(false)

      local property = Property.new(id, type, defval, self, errorHere)

      local need_semicolon = true

      if checkPattern "accessors" then
        if not type.allows_accessors then
          errorHere("this property type does not allow defining accessors")
        end

        while true do
          local id = expectIdentifier()

          expectToken "="

          accessor = expectPattern('".-"', "string for accessor pattern")
          accessor = accessor:sub(2,-2)
          if not accessor:find("%%") then
            errorHere(
              "accessor pattern must contain at least one '%' to replace "..
              "with the lookup")
          end

          property:addAccessor(id, accessor, errorHere)

          if not checkToken "," then
            if checkToken ";" then
              need_semicolon = false
              break
            else
              errorHere("expected a ';' or ',' after accessor definition")
            end
          end
        end
      end

      if need_semicolon then
        expectToken ";"
      end
    end

    if checkToken "}" then
      break
    end
  end

  -- util.dumpValue(self, 8)
end

-- * ==========================================================================
-- *   View 
-- * ==========================================================================

--- Provides a 'view' of a schema for use by code needing to inspect the 
--- values of schema properties.
View = makeType()
Schema.View = View

-- * --------------------------------------------------------------------------

View.new = function(schema)
  local o = {}
  o.schema = schema
  return setmetatable(o, View)
end

-- * --------------------------------------------------------------------------

View.__index = function(self, key)
  local property = 
    assert(self.schema.property.map[key],
      "no property named '"..key.."' in schema "..self.schema.name)

  return property.type:viewIndex(property)
end

-- * ==========================================================================
-- *   Property
-- * ==========================================================================

Property = makeType()
Schema.Property = Property

-- * --------------------------------------------------------------------------

Property.new = function(
    name, 
    type, 
    defval, 
    schema, 
    errcb)
  local o = {}
  o.name = name
  o.type = type
  o.defval = defval
  if type.allows_accessors then
    o.accessors = {}
  end
  if schema.property.map[name] then
    if aliased_property then
      errcb(
        "cannot alias as '"..name.."' as a property with this name already "..
        "exists")
    else
      errcb("a property with name '"..name.."' already exists")
    end
  end
  schema.property.map[name] = o
  schema.property.list:push(o)
  return setmetatable(o, Property)
end

-- * --------------------------------------------------------------------------

Property.addAccessor = function(self, name, val, errcb)
  if self.accessors[name] then
    errcb("an accessor with name '"..name.."' was already defined")
  end

  self.accessors[name] = val
end

-- * ==========================================================================
-- *   Lookup
-- * ==========================================================================

--- The result of looking up a property.
Lookup = makeType()
Schema.Lookup = Lookup

-- * --------------------------------------------------------------------------

Lookup.new = function(property, varname)
  local o = {}
  o.property = property
  o.varname = varname
  return setmetatable(o, Lookup)
end

-- * --------------------------------------------------------------------------

Lookup.__index = function(self, key)
  return self.property.type:lookupIndex(self, key)
end

-- * --------------------------------------------------------------------------

Lookup.__call = function(self, ...)
  return self.property.type:lookupCall(self, ...)
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
  o.elems.list:eachWithIndex(function(elem,i)
    o.elems.map[elem] = i-1
  end)
  return setmetatable(o, Enum)
end

-- * --------------------------------------------------------------------------

Enum.getTypeName = function()
  return "u32"
end

-- * --------------------------------------------------------------------------

Enum.viewIndex = function(self, property)
  local index = function(_, key)
    local elem = 
      assert(self.elems.map[key],
        "no element named '"..key.."' in property "..property.name)
    return function()
      return tostring(elem)
    end
  end
  return setmetatable({}, { __index=index })
end

-- * --------------------------------------------------------------------------

Enum.lookupCall = function(self, lookup)
  return lookup.varname
end

-- * --------------------------------------------------------------------------

Enum.set = function(self, property, val)
  local elem = 
    assert(self.elems.map[val],
      "no element named '"..val.."' in property "..property.name)
  return 'setAs<u64>("'..property.name..'"_str, '..elem..')'
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
  o.elems.list:eachWithIndex(function(elem,i)
    o.elems.map[elem] = "1 << "..(i-1)
  end)
  return setmetatable(o, Flags)
end

-- * --------------------------------------------------------------------------

Flags.getTypeName = function()
  return "u64"
end

-- * --------------------------------------------------------------------------

Flags.lookupIndex = function(self, lookup, key)
  local getElem = function(name)
    return assert(self.elems.map[name],
      "no element named '"..name.."' in property '"..lookup.property.name.."'")
  end
  local checkFlag = function(vname, name)
    return "(("..vname..") & ("..elem.."))"
  end
  if key == "test" then
    return function(name)
      return "(("..lookup.varname..") & ("..getElem(name).."))"
    end
  elseif key == "testAny" then
    return function(...)
      local buf = buffer.new()
      buf:put("((",lookup.varname,") & (")
      local first = true
      List{...}:each(function(name)
        if not first then
          buf:put(" | ")
        end
        buf:put("(", getElem(name), ")")
        first = false
      end)
      buf:put("))")
      return buf:get()
    end
  end
end

-- * --------------------------------------------------------------------------

Flags.viewIndex = function(self, property)
  local index = function(_, key)
    local elem = 
      assert(self.elems.map[key],
        "no element named '"..key.."' in property '"..property.name)
    return function()
      return elem
    end
  end
  return setmetatable({}, { __index=index })
end

-- * --------------------------------------------------------------------------

Flags.lookupCall = function(self, lookup)
  return lookup.varname
end

-- * --------------------------------------------------------------------------

Flags.set = function(self, property, val)
  local elem = 
    assert(self.elems.map[val],
      "no element named '"..val.."' in property "..property.name)
  return 'setAs<u32>("'..property.name..'"_str, '..elem..')'
end

-- * ==========================================================================
-- *   CType
-- * ==========================================================================

CType = makeType()
Schema.CType = CType
CType.allows_accessors = true

-- * --------------------------------------------------------------------------

CType.new = function(name)
  local o = {}
  o.name = name
  return setmetatable(o, CType)
end

-- * --------------------------------------------------------------------------

CType.getTypeName = function(self)
  return self.name
end

-- * --------------------------------------------------------------------------

CType.lookupIndex = function(self, lookup, key)
  local property = lookup.property 
  local accessor = property.accessors[key] 
  return function()
    if accessor then
      return accessor:gsub("%%", lookup.varname)
    else
      -- Assume accessing some var on the ctype
      return lookup.varname.."."..key
    end
  end
end

-- * --------------------------------------------------------------------------

CType.lookupCall = function(self, lookup)
  return lookup.varname
end

-- * --------------------------------------------------------------------------

CType.set = function(self, property, val)
  return 'setAs<'..self.name..'>("'..property.name..'"_str, '..val..')'
end

-- * ==========================================================================
-- *   Func
-- * ==========================================================================

Func = makeType()
Schema.Func = Func

-- * --------------------------------------------------------------------------

Func.new = function(id, def, schema, errcb)
  local o = {}
  o.id = id
  o.def = def
  if schema.funcs[id] then
    errcb("function with name '"..id.."' already exists")
  end
  schema.funcs[id] = o
  return setmetatable(o, Func)
end

-- * --------------------------------------------------------------------------

Func.call = function(self, item)
  local style = item.style
  local def = self.def
  local schema = item.schema
  
  -- This is black magic that I feel will break sooner or later.
  -- But its kinda awesome that it works.
  local buf = buffer.new()
  local first = def:find("[%%%$]")
  buf:put(def:sub(1, first-1))
  local prev_stop
  for start, kind, term, stop in self.def:gmatch "()([%%%$])%.([%w%.]+)()" do
    if kind == "$" then
      if prev_stop then
        buf:put(def:sub(prev_stop, start-1))
      end

      prev_stop = stop

      local st, sp = term:find("%w+")
      local func = schema.funcs[term:sub(st,sp)]
      if func then
        local res = func:call(item)
        buf:put(res)
      end

      if not func then
        buf:put(item.varname, "->", term)
      end
    else
      local res = style
      for prop in term:gmatch "(%w+)%.?" do
        res = res[prop]
      end
      res = res()
      
      if prev_stop then
        buf:put(def:sub(prev_stop, start-1))
      end

      prev_stop = stop

      buf:put(res)
    end
  end

  buf:put(def:sub(prev_stop))

  return buf:get()
end

return Schema
