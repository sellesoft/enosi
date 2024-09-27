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
  error("\nat "..line..":"..column..": "..makeStr(...), 2)
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

return Parser
