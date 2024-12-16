--- 
--- Helper for scanning text out of some input text.
---

local Type = require "Type"
local List = require "list"
local buffer = require "string.buffer"

local strbuf = buffer.new()

local str = function(...)
  strbuf:put(...)
  return strbuf:get()
end

---@class iro.Scanner
--- The current offset into the buffer.
---@field offset number
--- The text we are scanning.
---@field text string
local Scanner = Type.make()

-- * --------------------------------------------------------------------------

Scanner.new = function(text)
  local o = {}
  o.text = text
  o.offset = 1
  return setmetatable(o, Scanner)
end

-- * --------------------------------------------------------------------------

Scanner.errorHere = function(self, ...)
  local line = 1 + select(2, self.text:sub(1, self.offset):gsub("\n", ""))
  local column = #self.text:sub(1, self.offset):match("\n?.*$")
  local line_text = self.text:match "[^\n]+$"

  local message = str(...)

  error(
    str("\n at ",line,":",column,": ",message,"\n",
    line_text,"\n",
    (" "):rep(column),"^"))
end

-- * --------------------------------------------------------------------------

Scanner.pattern = function(self, pattern)
  return self.text:find(pattern, self.offset)
end

-- * --------------------------------------------------------------------------

Scanner.skipWhitespace = function(self)
  local found, stop = self:pattern "^%s*"
  if found then
    self.offset = stop + 1
  end
end

-- * --------------------------------------------------------------------------

Scanner.checkToken = function(self, token)
  self:skipWhitespace()
  local start, stop = self:pattern("^"..token)
  if not start then
    return false
  end
  self.offset = stop + 1
  return true
end

-- * --------------------------------------------------------------------------

Scanner.expectToken = function(self, token)
  if not self:checkToken(token) then
    self:errorHere("expected ",token)
  end
end

-- * --------------------------------------------------------------------------

Scanner.checkIdentifier = function(self)
  self:skipWhitespace()
  local start, stop = self:pattern("^[%w_]+")
  if not start then
    return 
  end
  self.offset = stop + 1
  return self.text:sub(start, stop)
end

-- * --------------------------------------------------------------------------

Scanner.expectIdentifier = function(self)
  local id = self:checkIdentifier()
  if not id then
    self:errorHere("expected an identifier")
end
  return id
end

-- * --------------------------------------------------------------------------

Scanner.checkPattern = function(self, pattern)
  self:skipWhitespace()
  local start, stop = self:pattern("^"..pattern)
  if not start then
    return
  end
  self.offset = stop + 1
  return self.text:sub(start, stop)
end

-- * --------------------------------------------------------------------------

Scanner.expectPattern = function(self, pattern, expected)
  local result = self:checkPattern(pattern)
  if not result then
    self:errorHere("expected ",expected)
  end
  return result
end

-- * --------------------------------------------------------------------------

Scanner.checkNumber = function(self)
  self:skipWhitespace()
  local attempt = self:checkPattern "%d+%.%d+"
  if not attempt then
    attempt = self:checkPattern "%d+"
  end
  return attempt
end

-- * --------------------------------------------------------------------------

Scanner.expectNumber = function(self)
  return self:checkNumber() or 
    self:errorHere("expected a number")
end

-- * --------------------------------------------------------------------------

Scanner.checkString = function(self)
  self:skipWhitespace()
  return self:checkPattern '".-"'
end

-- * --------------------------------------------------------------------------

Scanner.expectString = function(self)
  return self:checkString() or self:errorHere "expected a string"
end

return Scanner
