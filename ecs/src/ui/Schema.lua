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

local string_buffer = buffer.new()

local makeStr = function(...)
  string_buffer:put(...)
  return string_buffer:get()
end

local Schema,
      View,
      Property,
      InheritedProperty,
      Lookup,
      Enum,
      Flags,
      CType,
      Float,
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

-- Unique values.
Schema.inherit = {}

-- * --------------------------------------------------------------------------

Schema.new = function(name, def, file_offset, base)
  local o = setmetatable({}, Schema)
  o.name = name
  o.property = {}
  o.property.list = List{}
  o.property.map = {}
  o.base = base or false
  o.funcs = {}
  o.terminal = false
  o.makeParser = false
  o:parse(def, file_offset)
  return o
end

-- * --------------------------------------------------------------------------

Schema.findProperty = function(self, name)
  local property = self.property.map[name]
  if not property and self.base then
    property = self.base:findProperty(name)
  end 
  return property
end

-- * --------------------------------------------------------------------------

Schema.findFunc = function(self, name)
  local func = self.funcs[name]
  if not func and self.base then
    func = self.base:findFunc(name)
  end
  return func
end

-- * --------------------------------------------------------------------------

Schema.__index = function(self, key)
  local raw = Schema[key]
  if raw then
    return raw
  end

  return assert(self.property.map[key],
    makeStr("no property named '",key,"' in schema '",self.name,"'"))
end

-- * --------------------------------------------------------------------------

--- Parses 'def' into this schema.
Schema.parse = function(self, def, file_offset)
  local ui = require "ui.UI"
  local parser = require"Parser".new(def, file_offset)

  -- Tracks the last top-level parsing loop a kind of tailing information
  -- was found to disallow specifying them multiple times.
  local tail_loops = {}

  -- How many times the top-level parsing loop has iterated.
  local loop_count = 0

  --- Wrapper around error function to pass to type init functions.
  --- This kinda sucks since it will report this in the callstack 
  --- but im too lazy to fix that rn.
  local errcb = function(...)
    parser:errorHere(...)
  end

  local parseEnumOrFlags = function()
    local elems = List{}
    parser:expectToken "{"
    while true do
      elems:push(parser:expectIdentifier())
      
      if not parser:checkToken "," then
        parser:expectToken "}"
        return elems
      else
        if parser:checkToken "}" then
          return elems
        end
      end
    end
  end

  --- <identifier>: ...
  local parseProperty = function(id)
    ---
    --- Parse and create type information.
    ---

    local typename = parser:expectIdentifier()

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
      f32 = function()
        return Float.new()
      end
    }

    local type
    local handler = handlers[typename]
    if handler then
      type = handler()
    else
      -- assume this is some C++ type.
      type = CType.new(typename)
    end

    ---
    --- Parse default value.
    ---

    local defval

    if not parser:checkToken "=" then
      if typename ~= "flags" then
        parser:errorHere(
          "default value can only be omitted on flag properties for now")
      end
      -- Flags can omit '=' to indicate an initial value of 0 (no flags set).
      defval = "0"
    else
      if typename == "enum" or
         typename == "flags" then
        local elem = parser:expectIdentifier()
        if not type.elems.map[elem] then
          parser:errorHere(
            "default value specified as '"..elem.."' but this is not "..
            "defined")
        end
        defval = type.elems.map[elem]
      elseif typename == "str" then
        defval = 
          parser:expectPattern('".-"', "a string")
        defval = defval.."_str"
      elseif typename == "f32" then
        local attempt = parser:checkPattern("%d+%.%d+")
        if not attempt then
          attempt = parser:checkPattern("%d+")
        end
        if not attempt then
          parser:errorHere("expected an integer or float for default value")
        end
        defval = attempt
      else
        defval = type:valueParser(nil, parser)
      end
    end

    local property = Property.new(id, type, defval, self, errcb)

    ---
    --- Parse tailing information.
    ---

    while true do
      local found_tail = false

      local checkTail = function(keyword, tailParser)
        if parser:checkToken(keyword) then
          if tail_loops[keyword] == loop_count then
            parser:errorHere(keyword.." already parsed for this property")
          end
          tail_loops[keyword] = loop_count
          found_tail = true
          tailParser()
        end
      end

      checkTail("accessors", function()
        if not type.allows_accessors then
          parser:errorHere(
            "this property type does not allow defining accessors")
        end

        while true do
          local id = parser:expectIdentifier()

          parser:expectToken "="

          local accessor = 
            parser:expectPattern('".-"', "string for accessor pattern")
          accessor = accessor:sub(2,-2)
          if not accessor:find("%%") then
            parser:errorHere(
              "accessor pattern must contain at least one '%' to replace "..
              "with the lookup")
          end

          property:addAccessor(id, accessor, errcb)

          if not parser:checkToken "," then
            return
          end
        end
      end)

      checkTail("values", function()
        while true do
          local id = parser:expectIdentifier()

          parser:expectToken "="

          local res = List
          {
            { '".-"', function(res) return res:sub(2,-2) end },
            { "%b{}", function(res) return res end },
          }
          :findAndMap(function(handler)
            local res = parser:checkPattern(handler[1])
            if res then
              return handler[2](res)
            end
          end)

          if not res then
            parser:errorHere("expected a braced list or string for value")
          end

          property:addValue(id, res, errcb)

          if not parser:checkToken "," then
            return
          end
        end
      end)

      checkTail("parseValue", function()
        local def = parser:expectPattern("%b{}", 
                      "braced lua code for value parser definition")
        property.valueParser = def:sub(2,-2)
      end)

      if not found_tail then
        parser:expectToken ";"
        break
      end
    end

    return property
  end
  
  -- <identifier> = ...
  local parseAssignment = function(id)
    -- Check if this id has already been defined.
    if self.property.map[id] then
      parser:errorHere(
        "attempt to assign to already defined property '",id,"'")
    end

    if parser:checkToken "inherit" then
      -- Check if the given id exists in the base schema.
      local property = self.base.property.map[id]
      if not property then
        parser:errorHere(
          "property ",id," was not inherited from a base schema")
      end

      InheritedProperty.new(property, self)
    else
      parser:errorHere("overriding default values is not supported yet.")
    end

    parser:expectToken ";"
  end

  parser:expectToken "{"
  while true do
    loop_count = loop_count + 1

    if parser:checkToken "make" then
      self.makeParser = parser:expectPattern("%b{}", 
        "braced lua code for make parsing")
      self.makeParser = self.makeParser:sub(2,-2)
    elseif parser:checkToken "terminal" then
      self.terminal = true
      parser:expectToken ";"
    elseif parser:checkToken "func" then
      local id = parser:expectIdentifier()
      local def = parser:expectPattern("%b{}", "braced function definition")
      def = def:sub(2,-2)
      Func.new(id, def, self, errcb)
    else
      local id = parser:expectIdentifier()

      if parser:checkToken ":" then
        parseProperty(id)
      elseif parser:checkToken "=" then
        parseAssignment(id)
      end
    end

    if parser:checkToken "}" then
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
      makeStr("no property named '",key,"' in schema ",self.schema.name))

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
  if schema.property.map[name] then
      errcb("a property with name '"..name.."' already exists")
  end
  local o = {}
  o.name = name
  o.type = type
  o.defval = defval
  o.values = {}
  o.valueParser = false
  if type.allows_accessors then
    o.accessors = {}
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

-- * --------------------------------------------------------------------------

Property.addValue = function(self, name, val, errcb)
  if self.values[name] then
    errcb("a value with name '"..name.."' was already defined")
  end

  self.values[name] = self.type:adjustValue(val)
end

-- * ==========================================================================
-- *   InheritedProperty
-- * ==========================================================================

--- A property of a schema that is intended to be inherited from 
--- a base item by default.
InheritedProperty = makeType()
Schema.InheritedProperty = InheritedProperty

InheritedProperty.new = function(base_property, schema)
  local o = {}
  o.base_property = base_property
  schema.property.map[base_property.name] = o
  schema.property.list:push(o)
  return setmetatable(o, InheritedProperty)
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
        makeStr("no element named '",key,"' in property ",property.name))
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
      makeStr("no element named '",val,"' in property ",property.name))
  return elem
end

-- * --------------------------------------------------------------------------

Enum.adjustValue = function(self, val)
  local res = val
  for k,v in pairs(self.elems.map) do
    res = res:gsub(k, v)
  end
  return res
end

-- * --------------------------------------------------------------------------

Enum.valueParser = function(self, property, parser)
  local elem = parser:expectIdentifier()
  local val = 
    assert(self.elems.map[elem],
      makeStr("no element named '",elem,"' in property ",property.name))
  return val
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
      makeStr(
        "no element named '",name,"' in property '",lookup.property.name,"'"))
  end
  if key == "test" then
    return function(name)
      return makeStr("((",lookup.varname,") & (",getElem(name),"))")
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
        makeStr("no element named '",key,"' in property '",property.name))
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
  local buf = buffer.new()
  buf:put("(")

  local first = true

  for name in val:gmatch("[^|]+") do
    if not first then
      buf:put("|")
    end

    name = name:match("%S+")

    local elem = self.elems.map[name]

    if not elem then
      elem = property.values[name]
    end

    if not elem then
      error("no element or value named '"..name.."' in property "..
            property.name)
    end

    buf:put("(", elem, ")")

    first = false
  end

  buf:put(")")

  return buf:get()
end

-- * --------------------------------------------------------------------------

Flags.adjustValue = function(self, val)
  local res = val
  for k,v in pairs(self.elems.map) do
    res = res:gsub(k, v)
  end
  return makeStr("(", res, ")")
end

-- * --------------------------------------------------------------------------

Flags.valueParser = function(self, property, parser)
  local buf = buffer.new()
  buf:put("(")
  local first = true
  while true do
    local elem
    if first then
      elem = parser:expectIdentifier()
    else
      elem = parser:checkIdentifier()
      if not elem then
        break
      end
    end
    local val = 
      assert(self.elems.map[elem],
        makeStr("no element named '",elem,"' in property ", property.name))
    if not first then
      buf:put("|")
    end
    buf:put(val)
    first = false
  end
  buf:put(")")
  return buf:get()
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
  if self.name == "str" then
    return makeStr('"',val,'"_str')
  end
  return val
end

-- * --------------------------------------------------------------------------

CType.adjustValue = function(self, val)
  return val
end

-- * --------------------------------------------------------------------------

CType.valueParser = function(self, property, parser)
  local switch = 
  {
  vec2f = function()
    local x = parser:expectNumber()
    local y = parser:expectNumber()
    return "{"..x..","..y.."}"
  end,
  vec4f = function()
    local n0 = parser:expectNumber()
    local n1 = parser:checkNumber()
    if n1 then
      local n2 = parser:checkNumber()
      if n2 then
        local n3 = parser:checkNumber()
        if n3 then
          return "{"..n0..","..n1..","..n2..","..n3.."}"
        else
          return "{"..n1..","..n0..","..n1..","..n2.."}"
        end
      else
        return "{"..n1..","..n0..","..n1..","..n0.."}"
      end
    else
      return "{"..n0..","..n0..","..n0..","..n0.."}"
    end
  end,
  Color = function()
    if parser:checkPattern "0x" then
      local hex = parser:expectPattern("[%x]+", "hex for color")
      return "0x"..hex
    else
      local r = parser:expectNumber()
      local g = parser:expectNumber()
      local b = parser:expectNumber()
      local a = parser:checkNumber()
      if not a then
        a = "255"
      end
      return "{"..r..","..g..","..b..","..a.."}"
    end
  end,
  str = function()
    return '"'..parser:expectString(true)..'"_str'
  end,
  }

  local case = switch[self.name]
  if not case then
    parser:errorHere("unhandled CType")
  end

  return case()
end

-- * ==========================================================================
-- *   Float
-- * ==========================================================================

-- TODO(sushi) generate all the primitive types.
Float = makeType()
Schema.Float = Float

-- * --------------------------------------------------------------------------

Float.new = function()
  return setmetatable({}, Float)
end

-- * --------------------------------------------------------------------------

Float.getTypeName = function()
  return "f32"
end

-- * --------------------------------------------------------------------------

Float.lookupIndex = function(self, lookup, key)
  error("attempt to index a float property")
end

-- * --------------------------------------------------------------------------

Float.lookupCall = function(self, lookup)
  return lookup.varname
end

-- * --------------------------------------------------------------------------

Float.set = function(self, property, val)
  return val
end

-- * --------------------------------------------------------------------------

Float.adjustValue = function(self, val)
  return val
end

-- * --------------------------------------------------------------------------

Float.valueParser = function(self, property, parser)
  return parser:expectNumber()
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
  local schema = item.widget.schema_def
  
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
      local func = schema:findFunc(term:sub(st,sp))
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
