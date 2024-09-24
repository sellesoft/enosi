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
    makeStr("no property named '",key,"' in schema '",self.name,"'"))
end

-- * --------------------------------------------------------------------------

--- Parses 'def' into this schema.
Schema.parse = function(self, def, file_offset)
  local ui = require "ui.UI"

  local offset = 1

  local errorHere = function(...)
    local line, column = 
      lpp.getLineAndColumnFromOffset(file_offset + offset - 1)
    error("\nat "..line..":"..column..": "..makeStr(...), 2)
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
      return def:sub(start,stop)
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

    local defval

    -- Requiring a default value for now.
    -- TODO(sushi) make this optional.
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
    elseif typename == "str" then
      defval = 
        expectPattern('".-"', "a string")
      defval = defval.."_str"
    elseif typename == "f32" then
      local attempt = checkPattern("%d+%.%d+")
      if not attempt then
        attempt = checkPattern("%d+")
      end
      if not attempt then
        errorHere("expected an integer or float for default value")
      end
      defval = attempt
    else
      defval = 
        expectPattern("%b{}", "a braced initializer list (eg. {...})")
    end

    return id, type, defval
  end

  local tail_loops = {}

  local loop_count = 0

  expectToken "{"
  while true do
    loop_count = loop_count + 1

    if checkToken "inherit" then
      local basename = expectIdentifier()

      local base = ui.schema_defs[basename]
      if not base then
        errorHere("no schema with name '",basename,"'")
      end

      -- Take on all the properties and funcs of the base schema.
      self.property.list:pushList(base.property.list)
      for k,v in pairs(base.property.map) do
        self.property.map[k] = v
      end

      expectToken ";"
    elseif checkToken "func" then
      local id = expectIdentifier()
      local def = expectPattern("%b{}", "braced function definition")
      def = def:sub(2,-2)
      Func.new(id, def, self, errorHere)
    else
      local id, type, defval = parseProperty(false)

      local property = Property.new(id, type, defval, self, errorHere)

      while true do
        local found_tail = false

        local checkTail = function(keyword, parser)
          if checkToken(keyword) then
            if tail_loops[keyword] == loop_count then
              errorHere(keyword.." already parsed for this property")
            end
            tail_loops[keyword] = loop_count
            found_tail = true
            parser()
          end
        end

        checkTail("accessors", function()
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
              return
            end
          end
        end)

        checkTail("values", function()
          while true do
            local id = expectIdentifier()

            expectToken "="

            local res = List
            {
              { '".-"', function(res) return res:sub(2,-2) end },
              { "%b{}", function(res) return res end },
            }
            :findAndMap(function(handler)
              local res = checkPattern(handler[1])
              if res then
                return handler[2](res)
              end
            end)

            if not res then
              errorHere("expected a braced list or string for value")
            end

            property:addValue(id, res, errorHere)

            if not checkToken "," then
              return
            end
          end
        end)

        if not found_tail then
          expectToken ";"
          break
        end
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
  local o = {}
  o.name = name
  o.type = type
  o.defval = defval
  o.values = {}
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

-- * --------------------------------------------------------------------------

Property.addValue = function(self, name, val, errcb)
  if self.values[name] then
    errcb("a value with name '"..name.."' was already defined")
  end

  self.values[name] = self.type:adjustValue(val)
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
  local checkFlag = function(vname, name)
    return makeStr("((",vname,") & (",elem,"))")
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
  local elem = self.elems.map[val]

  if not elem then
    elem = property.values[val]
  end

  if not elem then
    error("no element or value named '"..val.."' in property "..property.name)
  end

  return elem
end

-- * --------------------------------------------------------------------------

Flags.adjustValue = function(self, val)
  local res = val
  for k,v in pairs(self.elems.map) do
    res = res:gsub(k, v)
  end
  return makeStr("(", res, ")")
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
  return val
end

-- * --------------------------------------------------------------------------

CType.adjustValue = function(self, val)
  return val
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
