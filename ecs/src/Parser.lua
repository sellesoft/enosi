---
---
--- Common functions and state for parsing.
---
---

local Type = require "Type"
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

---@class Parser
--- The offset into the file we've begun parsing at.
---@field file_offset number
--- The text we are parsing.
---@field text string
--- Offset into the text we are parsing.
---@field offset number
local Parser = Type.make()

-- * --------------------------------------------------------------------------

Parser.new = function(text, file_offset)
  local o = {}
  o.text = text
  o.file_offset = file_offset
  o.offset = 1
  return setmetatable(o, Parser)
end

-- * --------------------------------------------------------------------------

Parser.errorHere = function(self, ...)
  local line, column = 
    lpp.getLineAndColumnFromOffset(self.file_offset + self.offset - 1)

  local scan = self.offset

  print(scan)

  while true do
    if self.text:sub(scan,scan) == "\n" or
       scan == 1
    then
      break
    end

    scan = scan - 1
  end

  local start,stop = self.text:find("[^\n]+", scan)
  print(start, stop)

  error("\nat "..line..":"..column..": "..makeStr(...).."\n"..
        self.text:sub(start, stop).."\n"..
        (" "):rep(column).."^", 2)
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

return Parser
