local buffer = require "string.buffer"
local List = require "list"

--- Helper for generating C code in a consistently formatted way.
---@class CGen
--- The luajit string buffer that C code is output to.
---@field buffer any
--- The current indentation depth.
---@field depth number
local CGen = {}
CGen.__index = CGen

--- Creates a new CGen object.
---@return CGen
CGen.new = function()
  local o = {}
  setmetatable(o, CGen)
  o.buffer = buffer.new()
  o.depth = 0
  return o
end

CGen.__tostring = function(self)
  return tostring(self.buffer)
end

--- Appends indentation upto self.depth
CGen.appendIndentation = function(self)
  for _=1,self.depth do
    self.buffer:put "  "
  end
end

--- Starts a braced scope, incrementing depth and appening an open brace.
CGen.beginScope = function(self)
  self:appendIndentation()
  self.buffer:put "{\n"
  self.depth = self.depth + 1
end

--- Ends a previously begun scope.
---@param trailing string? Extra text inserted after the closing brace, but before the newline.
CGen.endScope = function(self, trailing)
  self.depth = self.depth - 1
  self:appendIndentation()
  if trailing then
    self.buffer:put("}", trailing, "\n")
  else
    self.buffer:put "}\n"
  end
end

--- Starts an 'enum class' with the given name
---@param name string
CGen.beginEnum = function(self, name)
  self:appendIndentation()
  self.buffer:put("enum class ", name, "\n")
  self:beginScope()
end

--- Ends a previously begun scope.
CGen.endEnum = function(self)
  self:endScope ";"
end

--- Adds an enum element, literally just ''<name>,'
CGen.appendEnumElement = function(self, name)
  self:appendIndentation()
  self.buffer:put(name, ",\n")
end

--- Begins a switch statement on the given 'cond'.
---@param cond string The 'condition' to switch on
CGen.beginSwitch = function(self, cond)
  self:appendIndentation()
  self.buffer:put("switch (", cond, ")\n")
  -- Manually place the scope because we dont want indentation for the cases.
  self:appendIndentation()
  self.buffer:put("{\n")
end

--- Ends a previously begun switch.
CGen.endSwitch = function(self)
  self:appendIndentation()
  self.buffer:put("}\n")
end

--- Begins a switch case on 'c'
CGen.beginCase = function(self, c)
  self:appendIndentation()
  self.buffer:put("case ", c, ":\n")
  self.depth = self.depth + 1
end

--- Ends a previously begun case.
CGen.endCase = function(self)
  self:appendIndentation()
  self.buffer:put("break;\n")
  self.depth = self.depth - 1
end

--- Begins a default case.
CGen.beginDefaultCase = function(self)
  self:appendIndentation()
  self.buffer:put "default:\n"
  self.depth = self.depth + 1
end

--- Ends a previously begun default case.
CGen.endDefaultCase = CGen.endCase

--- Starts an if statement using 'cond' as its condition.
---@param cond string
CGen.beginIf = function(self, cond)
  self:appendIndentation()
  self.buffer:put("if (", cond, ")\n")
  self:beginScope()
end

--- Ends a previously begun 'if'.
CGen.endIf = function(self)
  self:endScope()
end

--- Begins an 'else'.
CGen.beginElse = function(self)
  self:endIf()
  self:appendIndentation()
  self.buffer:put("else\n")
  self:beginScope()
end

--- Begins an 'else if' using 'cond' as its condition.
---@param cond string
CGen.beginElseIf = function(self, cond)
  self:endIf()
  self:appendIndentation()
  self.buffer:put("else if (", cond, ")\n")
  self:beginScope()
end

--- Begins a function definition.
---@param return_type string
---@param name string
--- The function parameters
---@param ... string
CGen.beginFunction = function(self, return_type, name, ...)
  self:appendIndentation()
  self.buffer:put(return_type, " ", name, "(")
  local args = List{...}
  args:eachWithIndex(function(arg, i)
    self.buffer:put(arg)
    if i ~= args:len() then
      self.buffer:put(",")
    end
  end)
  self.buffer:put(")\n")
  self:beginScope()
end

--- Ends a function definition.
CGen.endFunction = function(self)
  self:endScope()
end

--- Begins a struct definition.
---@param name string?
CGen.beginStruct = function(self, name)
  self:appendIndentation()
  self.buffer:put("struct ")
  if name then
    self.buffer:put(name)
  end
  self.buffer:put("\n")
  self:beginScope()
end

--- Ends a struct definition
---@param declarator string?
CGen.endStruct = function(self, declarator)
  if declarator then
    self:endScope(" "..declarator..";")
  else
    self:endScope(";")
  end
end

--- Writes a struct member.
---@param type string
---@param name string
---@param default_value string?
CGen.appendStructMember = function(self, type, name, default_value)
  self:appendIndentation()
  self.buffer:put(type, " ", name)
  if default_value then
    self.buffer:put(" = ", default_value)
  end
  self.buffer:put ";\n"
end

--- Begins a union definion.
---@param name string?
CGen.beginUnion = function(self, name)
  self:appendIndentation()
  self.buffer:put("union ")
  if name then
    self.buffer:put(name)
  end
  self.buffer:put("\n")
  self:beginScope()
end

--- Ends a union definition.
---@param declarator string?
CGen.endUnion = function(self, declarator)
  if declarator then
    self:endScope(" "..declarator..";")
  else
    self:endScope(";")
  end
end

--- Writes a union member.
---@param type string
---@param name string
---@param default_value string?
CGen.appendUnionMember = function(self, type, name, default_value)
  self:appendIndentation()
  self.buffer:put(type, " ", name)
  if default_value then
    self.buffer:put(" = ", default_value)
  end
  self.buffer:put(";\n")
end

--- Writes a typedef.
---@param from string The type we are aliasing.
---@param to string The name of the new type.
CGen.typedef = function(self, from, to)
  self:appendIndentation()
  self.buffer:put("typedef ", from, " ", to, ";\n")
end

--- Appends the given args as a line of C code.
---@param ... any
CGen.append = function(self, ...)
  self:appendIndentation()
  self.buffer:put(...)
  self.buffer:put("\n")
end

return CGen
