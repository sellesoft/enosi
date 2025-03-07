local List = require "List"
local Type = require "Type"
local string_buffer = require "string.buffer"

local json = {}

local Parser = Type.make()

-- * --------------------------------------------------------------------------

Parser.new = function(text)
  local o = {}
  o.text = text
  o.offset = 1
  return setmetatable(o, Parser)
end

-- * --------------------------------------------------------------------------

Parser.errorHere = function(self, ...)
  local scan = self.offset

  while true do
    if self.text:sub(scan,scan) == '\n' or 
       scan == 1
    then
      break
    end
    scan = scan - 1
  end 

  print(scan)
  local start, stop = self.text:find("[^\n]+", scan)

  local _, line = self.text:sub(1, scan):gsub("[^\n]+", "")
  local column = self.offset - scan - 1

  local msg = "at "..line..":"..column..": "

  for arg in List{...}:each() do
    msg = msg..arg
  end

  msg = msg.."\n"..self.text:sub(start, stop).."\n"..
        (" "):rep(column).."^"

  error(msg, 2)
end

-- * --------------------------------------------------------------------------

Parser.skipWhitespace = function(self)
  local found, stop = self.text:find("^%s*", self.offset)
  if found then
    self.offset = stop + 1
  end
end

-- * --------------------------------------------------------------------------

Parser.expectToken = function(self, token)
  self:skipWhitespace()
  local start, stop = self.text:find("^"..token, self.offset)
  if not start then
    self:errorHere("expected '"..token.."'")
end
  self.offset = stop + 1
end

-- * --------------------------------------------------------------------------

Parser.expectIdentifier = function(self)
  self:skipWhitespace()
  local start, stop = self.text:find("^[%w_]+", self.offset)
  if not start then
    self:errorHere("expected identifier")
  end
  local id = self.text:sub(start, stop)
  self.offset = stop + 1
  if not id then
    self:errorHere("failed to sub identifier at offsets ",start," ",stop)
  end
  return id
end

-- * --------------------------------------------------------------------------

Parser.expectPattern = function(self, pattern, expected)
  self:skipWhitespace()
  local start, stop = self.text:find("^"..pattern, self.offset)
  if not start then
    self:errorHere("expected "..expected)
  end
  self.offset = stop + 1
  return self.text:sub(start, stop)
end

-- * --------------------------------------------------------------------------

Parser.expectNumber = function(self)
  self:skipWhitespace()
  local attempt = self:checkPattern("%d+%.%d+")
  if not attempt then
    attempt = self:checkPattern("%d+")
  end
  if not attempt then
    self:errorHere("expected a number")
  end
  return attempt
end

-- * --------------------------------------------------------------------------

Parser.expectString = function(self)
  self:skipWhitespace()
  local result = self:checkPattern('".-"')
  if not result then
    self:errorHere("expected a string")
  end
  return result:sub(2, -2)
end

-- * --------------------------------------------------------------------------

Parser.checkToken = function(self, token)
  self:skipWhitespace()
  local start, stop = self.text:find("^"..token, self.offset)
  if start then
    self.offset = stop + 1
    return self.text:sub(start, stop)
  end
end

-- * --------------------------------------------------------------------------

Parser.checkPattern = function(self, pattern)
  self:skipWhitespace()
  local start, stop = self.text:find("^"..pattern, self.offset)
  if start then
    self.offset = stop + 1
    return self.text:sub(start, stop)
  end
end

-- * --------------------------------------------------------------------------

Parser.checkIdentifier = function(self)
  self:skipWhitespace()
  local start, stop = self.text:find("^[%w_]+", self.offset)
  if start then
    self.offset = stop + 1
    return self.text:sub(start, stop)
  end
end

-- * --------------------------------------------------------------------------

Parser.checkNumber = function(self)
  self:skipWhitespace()
  local attempt = self:checkPattern("%d+%.%d+")
  if attempt then
    return attempt
  end
  attempt = self:checkPattern("%d+")
  if attempt then
    return attempt
  end
end

-- * --------------------------------------------------------------------------
 
Parser.checkString = function(self)
  self:skipWhitespace()

  if self.text:sub(self.offset,self.offset) == '"' then
    local start = self.offset + 1
    local stop = start
    local skip = false
    while true do
      local cur = self.text:sub(stop, stop)
      if skip then
        skip = false
      elseif cur == '\\' then
        skip = true
      elseif cur == '"' then
        self.offset = stop + 1
        return self.text:sub(start, stop-1)
      else
        if self.offset > #self.text then
          self:errorHere("unterminated string")
        end
      end
      stop = stop + 1
    end
  end
end

-- * --------------------------------------------------------------------------

--- Decodes a given string of json into a lua table.
---
---@return table
json.decode = function(text)
  local parser = Parser.new(text)

  parser:skipWhitespace()

  local parseObject,
        parseArray,
        parseValue

  parseObject = function()
    local obj = {}

    if parser:checkToken "}" then
      return obj
    end

    while true do 
      local member_name = parser:expectPattern('".-"', "member name")

      member_name = member_name:sub(2,-2)

      parser:expectToken ":"

      local val = parseValue()

      obj[member_name] = val

      if parser:checkToken "}" then
        return obj
      end

      parser:expectToken ","
    end
  end

  parseArray = function()
    local arr = List{}

    if parser:checkToken "]" then
      return arr
    end

    while true do
      arr:push(parseValue())

      if parser:checkToken "]" then
        return arr
      end

      parser:expectToken ","
    end
  end

  parseString = function(s)
    local result = string_buffer.new()
    local offset = 1

    while offset <= #s do
      local x = s:byte(offset)

      if x == 92 then -- '\' - escape character
        offset = offset + 1
        local c = s:sub(offset,offset)
        if '"' == c then
          result:put '"'
        elseif '\\' == c then
          result:put '\\'
        elseif '/' == c then
          result:put '/'
        elseif 'n' == c then
          result:put "\n"
        else
          -- TODO(sushi) handle unicode and whatever else later when needed.
          error("unhandled escape char "..s:sub(offset,offset))
        end
      else
        result:put(string.char(x))
      end

      offset = offset + 1
    end

    return result:get()
  end

  parseValue = function()
    local val
    if parser:checkToken "null" then
      val = "null"
    elseif parser:checkToken "true" then
      val = true
    elseif parser:checkToken "false" then
      val = false
    else
      val = parser:checkNumber()
      if not val then
        val = parser:checkString()
        if val then
          val = parseString(val)
        end
      else
        val = tonumber(val)
      end

      if not val then
        if parser:checkToken "%[" then
          val = parseArray()
        elseif parser:checkToken "{" then
          val = parseObject()
        end
      end
    end
    
    if val == nil then
      parser:errorHere("expected a value")
    end

    return val
  end

  return parseValue()
end

-- * --------------------------------------------------------------------------

--- Encodes a given lua value into json.
json.encode = function(val)
  local buffer =  string_buffer.new()

  local depth = 0

  local indent = function()
    for i=1,depth do
      buffer:put "  "
    end
  end

  local nudgeDepth = function(x)
    depth = depth + x
  end

  local encodeList,
        encodeTable,
        encodeValue

  encodeList = function(val)
    buffer:put "[\n"
    nudgeDepth(1)

    local count = #val
    for val,i in val:eachWithIndex() do
      indent()
      encodeValue(val)
      if i ~= count then
        buffer:put ","
      end
      buffer:put "\n"
    end

    nudgeDepth(-1)
    indent()
    buffer:put "]"
  end

  encodeTable = function(val)
    buffer:put "{\n"
    nudgeDepth(1)

    local first = true
    for k,v in pairs(val) do
      if type(k) ~= "string" then
        error("table keys must be strings, got "..type(k)..
              "\ntable was: \n"..require "Util" .dumpValue(val, 2))
      end

      if not first then
        buffer:put ",\n"
      else
        first = false
      end

      indent()
      buffer:put('"', k, '": ')
      encodeValue(v, true)
    end

    nudgeDepth(-1)
    buffer:put "\n"
    indent()
    buffer:put("}")
  end

  encodeValue = function(val, is_member_value)
    if type(val) == "number" then
      buffer:put(tostring(val))
    elseif type(val) == "boolean" then
      buffer:put(tostring(val))
    elseif type(val) == "string" then
      local sanitized = val
      sanitized = sanitized:gsub('"', '\\"')
      buffer:put('"', sanitized, '"')
    elseif type(val) == "table" then
      if is_member_value then
        buffer:put "\n"
        indent()
      end
      if List:isTypeOf(val) then
        encodeList(val)
      else
        encodeTable(val)
      end
    else
      error(require "Util" .dumpValueToString(val).."\n"..
            "unexpected lua type in encodeValue, only numbers, strings, "..
            "iro.Lists, booleans and tables are supported")
    end
  end

  encodeValue(val)

  return buffer:get()
end

return json
