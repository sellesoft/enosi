---
--- glue
---

local lpp = require "lpp"
local reflect = require "reflection.Reflector"
local ui = require "ui.UI"
local buffer = require "string.buffer"
local Placeable = require "ui.Placeable"
local List = require "list"
local CGen = require "cgen"
local glob = require "Glob"
local log = require "logger" ("ui.inspector", Verbosity.Info)
local LuaType = require "Type"
local util = require "util"

local Inspector = {}

Inspector.structs =
{
  map = {},
  list = List{}
}
Inspector.enums = 
{
  map = {},
  list = List{}
}
Inspector.types = 
{
  map = {},
  list = List{},
}

local addType = function(name, sub, o)
  if Inspector.types.map[name] then
    return Inspector.types.map[name]
  end
  o.type_idx = Inspector.types.list:len()
  Inspector.types.map[name] = o
  Inspector.types.list:push(o)
  if sub then
    sub.map[name] = o
    sub.list:push(o)
  end
end

local Void,
      Scalar,
      CArray,
      Pointer,
      Struct,
      Union,
      Enum,
      Member

local clang = reflect.createContext()

Void = LuaType.make()
Inspector.Void = Void

Void.is_builtin = true

addType("void", nil, setmetatable({name="void"}, Void))

Scalar = LuaType.make()
Inspector.Scalar = Scalar

Scalar.new = function(name, decl)
  local o = {}
  addType(name, nil, o)
  o.name = name
  o.canonical_name = name
  o.cdecl = decl
  o.is_builtin = true
  assert(decl)
  return setmetatable(o, Scalar)
end

CArray = LuaType.make()
Inspector.CArray = CArray

CArray.new = function(canonical_name, len, elemtype)
  local o = {}
  o.name = canonical_name
  local existing = addType(canonical_name, nil, o)
  if existing then
    return existing
  end
  o.len = len
  o.elemtype = elemtype
  return setmetatable(o, CArray)
end

Pointer = LuaType.make()
Inspector.Pointer = Pointer

Pointer.new = function(canonical_name, subtype)
  local o = {}
  o.name = canonical_name
  local existing = addType(canonical_name, nil, o)
  if existing then
    return existing
  end
  o.subtype = subtype
  return setmetatable(o, Pointer)
end

---@class Struct
Struct = LuaType.make()
Inspector.Struct = Struct

---@return Struct
Struct.new = function(
    name,
    canonical_name,
    decl,
    member_path)
  local o = {}
  local existing = addType(canonical_name, Inspector.structs, o)
  if existing then
    return existing
  end

  o.name = name
  o.canonical_name = canonical_name
  o.is_anonymous = decl:isAnonymous()
  o.cdecl = decl
  o.members = List{}
  o.member_path = member_path

  return setmetatable(o, Struct)
end

Struct.addMember = function(self, member)
  self.members:push(member)
end

Struct.walkMembers = function(self, bf, df)
  local function walk(member)
    if Struct.isTypeOf(member) then
      member.children:each(function(child)
        if bf then bf(child) end
        walk(child)
        if df then df(child) end
      end)
    end
  end

  self.members:each(function(member)
    if bf then bf(member) end
    walk(member)
    if df then df(member) end
  end)
end

Union = LuaType.make()
Inspector.Union = Union

Union.new = function(name, canonical_name, decl, member_path)
  local o = {}
  local existing = addType(canonical_name, nil, o)
  if existing then
    return existing
  end

  o.name = name
  o.canonical_name = canonical_name
  o.is_anonymous = decl:isAnonymous()
  o.cdecl = decl
  o.members = List{}
  o.member_path = member_path

  return setmetatable(o, Union)
end

Union.addMember = function(self, m)
  self.members:push(m)
end

---@class Enum
Enum = LuaType.make()
Inspector.Enum = Enum

Enum.new = function(name, canonical_name, decl)
  local o = {}
  local existing = addType(canonical_name, Inspector.enums, o)
  if existing then
    return existing
  end
  o.name = name
  o.canonical_name = canonical_name
  o.elems = List{}
  o.cdecl = decl
  return setmetatable(o, Enum)
end

Enum.addElem = function(self, elem)
  self.elems:push(elem)
end

Member = LuaType.make()
Inspector.Member = Member

Member.new = function(name, type, idx, offset, is_anonymous, is_arr_or_ptr)
  local o = {}
  o.name = name
  o.type = type
  o.idx = idx
  o.is_anonymous = is_anonymous
  o.offset = offset
  o.is_arr_or_ptr = is_arr_or_ptr or false
  return setmetatable(o, Member)
end

Inspector.importHeaders = function()
  local buf = buffer.new()

  glob"**/*.lh":each(function(path)
    local result = lpp.import(path:sub(#"src/"+1))
    if result then
      buf:put(result)
    end
  end)

  clang:parseString(tostring(buf))

  local makeScalars = function(l)
    List(l):each(function(x)
      local type = clang:lookupType(x)
      Scalar.new(x, type)
    end)
  end

  local scalar_types = List
  {
    "float", "double"
  }

  List
  {
    "char", "short", "int", "long"
  }
  :each(function(i)
    scalar_types:push(i)
    scalar_types:push("signed "..i)
    scalar_types:push("unsigned "..i)
  end)

  makeScalars(scalar_types)

  local function collectEnum(edecl)
    local type = edecl:getTypeDeclType()
    local existing = Inspector.types.map[type:getCanonicalTypeName()]
    if existing then
      return existing
    end
    local enum = 
      Enum.new(
        type:getTypeName(), 
        type:getCanonicalTypeName(),
        edecl)
    local iter = edecl:getEnumIter()
    if iter then
      while true do
        local elem = iter:next()
        if not elem then break end

        enum:addElem(elem:name())
      end
    end
    return enum
  end

  local collectStruct,
        collectUnion

  local function collectMembers(decl, type, member_path, member_is_ptr_or_arr)
    local miter = decl:getFieldIter()
    type.total_member_count = 0
    if miter then
      while true do
        local m = miter:next()
        if not m then break end

        local midx = type.total_member_count
        type.total_member_count = type.total_member_count + 1
        
        if m:isStruct() then
          -- TODO(sushi) this should never happen (i think)
          collectStruct(m, member_path..m:name())
        else
          local mtype 
          local mctype = m:type()

          local is_ptr = mctype:isPointer()
          local is_arr = mctype:isArray()

          local resolveType = function(rtype)
            local typename = rtype:getCanonicalTypeName()
            local itype = Inspector.types.map[typename]
            if not itype then
              local next_path
              local is_anon = false
              if member_path then
                if m:isAnonymousField() then
                  next_path = member_path
                  is_anon = true
                else
                  local access = member_is_ptr_or_arr or "."
                  next_path = member_path..access..m:name()
                end
              else
                local name = type.name or type.canonical_name
                if m:isAnonymousField() then
                  is_anon = true
                end
                next_path = name.."::"..m:name()
              end

              local ptr_or_arr
              if is_anon then
                ptr_or_arr = member_is_ptr_or_arr
              else
                if is_ptr then
                  ptr_or_arr = "->"
                elseif is_arr then
                  ptr_or_arr = "[0]."
                end
              end

              local typedef = rtype:getDecl()
              if typedef then
                if typedef:isStruct() then
                  collectStruct(typedef, next_path, ptr_or_arr)
                elseif typedef:isEnum() then
                  collectEnum(typedef)
                elseif typedef:isUnion() then
                  collectUnion(typedef, next_path, ptr_or_arr)
                end
              end
            end
            return Inspector.types.map[typename]
          end

          if is_ptr then
            local subtype = mctype:getPointeeType()
            local isubtype = resolveType(subtype)
            mtype = Pointer.new(mctype:getCanonicalTypeName(), isubtype)
          elseif is_arr then
            local subtype = mctype:getArrayElementType()
            local isubtype = resolveType(subtype)
            local len = mctype:getArrayLen()
            mtype = CArray.new(mctype:getCanonicalTypeName(), len, isubtype)
          else 
            mtype = resolveType(mctype)
          end

          if not mtype then
            error("failed to resolve type of member "..m:name().." ("..
                  mctype:getCanonicalTypeName()..")")
          end

          local is_arr_or_ptr = 
            is_arr and "arr" or
            is_ptr and "ptr" or
            nil

          local member = 
            Member.new(
              m:name(), 
              mtype, 
              midx,
              m:getFieldOffset(),
              m:isAnonymousField(),
              is_arr_or_ptr)
          type:addMember(member)
        end
      end
    end
  end

  collectUnion = function(udecl, member_path, member_is_ptr_or_arr)
    local type = udecl:getTypeDeclType()
    local existing = Inspector.types.map[type:getCanonicalTypeName()]
    if existing then
      return existing
    end
    local union = 
      Union.new(
        udecl:name(),
        type:getCanonicalTypeName(),
        udecl,
        member_path)
    collectMembers(udecl, union, member_path, member_is_ptr_or_arr)
    return union
  end

  collectStruct = function(sdecl, member_path, member_is_ptr_or_arr)
    local defdecl = sdecl:getDefinition()
    if defdecl then
      sdecl = defdecl
    end
    if sdecl:isTemplate() then
      return
    end
    local type = sdecl:getTypeDeclType()
    if not type then
      sdecl:dump()
    end
    type:makeComplete()
    local existing = Inspector.types.map[type:getCanonicalTypeName()]
    if existing then
      return existing
    end
    local struct = 
      Struct.new(
        sdecl:name(),
        type:getCanonicalTypeName(),
        sdecl,
        member_path)
    collectMembers(sdecl, struct, member_path, member_is_ptr_or_arr)
    struct.complete = sdecl:isComplete()
    return struct
  end

  local tu = clang:getTranslationUnitDecl()
  local iter = tu:getDeclIter()
  while true do
    local decl = iter:next()
    if not decl then break end

    -- TODO(sushi) global vars if we ever use them outside of cpp files.
    if decl:name() ~= "" then
      local idecl
      if decl:isStruct() then
        if decl:name() ~= "" then
          idecl = collectStruct(decl)
        end
      elseif decl:isUnion() then
        idecl = collectUnion(decl)
      elseif decl:isEnum() then
        idecl = collectEnum(decl)
      end

      if idecl then
        idecl.is_top_level = true
      end
    end
  end

  Inspector.types.list:each(function(type)
    if Struct.isTypeOf(type) then
      --log:info(type.canonical_name, "\n")
    end
  end)

  return buf:get()
end

return Inspector
