---
--- Internal representation of clang's AST.
---

local LuaType = require "Type"
local List = require "List"
local buffer = require "string.buffer"

local PrettyPrinter = LuaType.make()

PrettyPrinter.new = function()
  local o = {}
  o.depth = 0
  o.buf = buffer.new()
  return setmetatable(o, PrettyPrinter)
end

PrettyPrinter.write = function(self, ...)
  for arg in List{...}:each() do
    arg = tostring(arg):gsub("\n", "\n"..(" "):rep(self.depth))
    self.buf:put(arg)
  end
end

PrettyPrinter.incDepth = function(self)
  self.depth = self.depth + 1
end

PrettyPrinter.decDepth = function(self)
  self.depth = self.depth - 1
end

local ast = {}

ast.types = 
{
  list = List{},
  map = {}
}

ast.decls = 
{
  list = List{},
  map = {}
}

local Type = LuaType.make()
local Decl = LuaType.make()

Type.metadata = {}
Decl.metadata = {}

-- Automatically set the name of each type.
setmetatable(ast, 
{
  __newindex = function(_,k,v)
    if Type:isTypeOf(v) or Decl:isTypeOf(v) then
      v.ast_kind = k
    end
    rawset(ast, k, v)
  end
})

ast.Type = Type

Type.dump = function(self, depth)
  error("unimplemented dump on Type "..self.ast_kind)
end

Type.getRecordDecl = function(self)
  local type = self
  if type:is(ast.ElaboratedType) then
    type = type.desugared
  end
  if type:is(ast.TagType) then
    if type.decl:is(ast.Record) then
      return type.decl
    end
  end
end

Type.getEnumDecl = function(self)
  local type = self
  if type:is(ast.ElaboratedType) then
    type = type.desugared
  end
  if type:is(ast.TagType) then
    if type.decl:is(ast.Enum) then
      return type.decl
    end
  end
end

Type.getDecl = function(self)
  local type = self:getDesugared()
  if type:is(ast.TagType) then
    return type.decl
  end
end

Type.getDesugared = function(self)
  local type = self
  if type:is(ast.ElaboratedType) then
    type = type.desugared
  end
  return type
end

local BuiltinType = Type:derive()
ast.BuiltinType = BuiltinType

BuiltinType.new = function(name, size) 
  local o = BuiltinType:derive()
  o.name = name
  o.size = size
  BuiltinType[name] = o
  return o
end

BuiltinType.tostring = function(self)
  return "BuiltinType("..self.name..","..self.size..")"
end

BuiltinType.new("void", 0)

local scalar_types = List
{
  { "float", 4 }, { "double", 8 },
}

List
{
  { "char",  1 }, 
  { "short", 2 },
  { "int",   4 },
  { "long",  8 },
}
:each(function(i)
  scalar_types:push{ i[1], i[2] }
  scalar_types:push{ "signed "..i[1], i[2] }
  scalar_types:push{ "unsigned "..i[1], i[2] }
end)

scalar_types:each(function(st)
  BuiltinType.new(st[1], st[2])
end)

local PointerType = Type:derive()
ast.PointerType = PointerType

PointerType.new = function(subtype)
  local o = {}
  o.subtype = subtype
  return setmetatable(o, PointerType)
end

PointerType.tostring = function(self)
  local subtype
  if self.subtype then
    subtype = self.subtype:tostring()
  else
    subtype = "nil"
  end
  return "PointerType("..subtype..")"
end

PointerType.tryUnwrapToRecordDecl = function(self)
  local type = self.subtype
  while true do
    if type:is(ast.ElaboratedType) then
      type = type.desugared
    elseif type:is(ast.TagType) then
      local decl = type.decl
      if decl:is(ast.Record) then
        return decl
      else 
        return
      end
    else
      return
    end
  end
end

local ReferenceType = Type:derive()
ast.ReferenceType = ReferenceType

ReferenceType.new = function(subtype)
  local o = {}
  o.subtype = subtype
  return setmetatable(o, ReferenceType)
end

ReferenceType.tostring = function(self)
  return "ReferenceType("..self.subtype:tostring()..")"
end

local FunctionPointerType = Type:derive()
ast.FunctionPointerType = FunctionPointerType

FunctionPointerType.new = function()
  return setmetatable({}, FunctionPointerType)
end

FunctionPointerType.tostring = function(self)
  return "FunctionPointerType"
end

local CArray = Type:derive()
ast.CArray = CArray

CArray.new = function(subtype, len)
  local o = {}
  o.subtype = subtype
  o.len = len
  return setmetatable(o, CArray)
end

CArray.tostring = function(self)
  return "CArray("..self.subtype:tostring()..")"
end

local TagType = Type:derive()
ast.TagType = TagType

TagType.new = function(name, decl)
  local o = TagType:derive()
  o.name = name
  o.decl = decl
  return o
end

TagType.tostring = function(self)
  return "TagType("..self.name..")"
end

local TagDecl = Decl:derive()
ast.TagDecl = TagDecl

TagDecl.new = function(name)
  local o = {}
  o.name = name
  return setmetatable(o, TagDecl)
end

local ElaboratedType = Type:derive()
ast.ElaboratedType = ElaboratedType

ElaboratedType.new = function(name, desugared)
  local o = {}
  o.name = name
  o.desugared = desugared
  return setmetatable(o, ElaboratedType)
end

ElaboratedType.tostring = function(self)
  return "ElaboratedType("..self.name..","..self.desugared:tostring()..")"
end

local Enum = TagDecl:derive()
ast.Enum = Enum

Enum.new = function(name)
  local o = TagDecl.new(name)
  o.elems = List{}
  return setmetatable(o, Enum)
end

Enum.dump = function(self, out)
  local buf = out or PrettyPrinter.new()

  buf:write("\nEnum(", self.name, ")\n{\n")

  for elem in self.elems:each() do
    buf:write("  ", elem, "\n")
  end

  buf:write("}")

  if not out then
    io.write(buf.buf:get())
  end
end

local Record = TagDecl:derive()
ast.Record = Record

Record.new = function(name)
  local o = TagDecl.new(name)
  o.members = 
  {
    list = List{},
    map = {},
  }
  o.bases = List{}
  o.derived = List{}
  return setmetatable(o, Record)
end

Record.addMember = function(self, name, obj)
  self.members.map[name] = obj
  self.members.list:push{ name=name, obj=obj }
end

Record.tostring = function(self)
  return self.ast_kind.."("..self.name..")"
end

Record.__tostring = Record.tostring

Record.dump = function(self, out)
  local buf = out or PrettyPrinter.new()

  buf:write("\n",self:tostring(),"\n{")
  buf:incDepth()

  for member in self.members.list:each() do
    if member.obj:is(ast.Field) then
      buf:write("\n", member.obj.type:tostring(), " ", member.name)
    else
      member.obj:dump(buf)
    end
  end
  
  buf:decDepth()
  buf:write("\n}")

  if not out then
    io.write(buf.buf:get(), "\n")
  end
end

Record.getFieldCount = function(self)
  local count = 0
  for mem in self.members.list:each() do
    if mem.obj:is(ast.Field) then
      count = count + 1
    end
  end
  return count
end

Record.eachFieldWithIndex = function(self)
  local iter = self.members.list:eachWithIndex()
  return function()
    while true do
      local member,i = iter()
      if not member then
        return
      end
      if member.obj:is(ast.Field) then
        return member.obj, i-1
      end
    end
  end
end

Record.log = function(self, var)
  local s = 'INFO("'..var..' '..self.name..'"'
  for field in self.members.list:each() do
    s = s..', "\\n'..field.name..': ", '..var..'.'..field.name
  end
  return s..', "\\n")'
end

Record.isDerivedFrom = function(self, decl)
  for base in self.bases:each() do
    if base == decl or base:isDerivedFrom(decl) then
      return true
    end
  end
end

Record.hasMethod = function(self, name)
  local member = self.members.map[name]
  if member and member:is(ast.Function) then
    return true
  end
end 

local Field = Decl:derive()
ast.Field = Field

Field.new = function(name, type, offset)
  local o = {}
  o.name = name
  o.type = type
  o.offset = offset
  return setmetatable(o, Field)
end

local Struct = Record:derive()
ast.Struct = Struct

Struct.new = function(name)
  local o = Record.new(name)
  return setmetatable(o, Struct)
end

local Union = Record:derive()
ast.Union = Union

Union.new = function(name)
  local o = Record.new(name)
  return setmetatable(o, Union)
end

local TemplateSpecialization = Record:derive()
ast.TemplateSpecialization = TemplateSpecialization

TemplateSpecialization.new = function(name, specialized_name)
  local o = Record.new(name)
  o.template_args = List{}
  o.specialized_name = specialized_name
  return setmetatable(o, TemplateSpecialization)
end

TemplateSpecialization.tostring = function(self)
  local buf = buffer.new()
  buf:put("TemplateSpecialization("..self.specialized_name)
  for arg in self.template_args:each() do
    buf:put ","
    if type(arg) == "number" then
      buf:put(arg)
    else
      buf:put(arg:tostring())
    end
  end
  buf:put ")"
  return buf:get()
end

local Function = Decl:derive()
ast.Function = Function

Function.new = function(name)
  local o = {}
  o.name = name
  return setmetatable(o, Function)
end

Function.tostring = function(self)
  return "Function("..self.name..")"
end

return ast
