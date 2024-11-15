---
--- Generates our internal representation of C types and decls and what not 
--- from a given input string.
---
--- We do this to avoid interacting with clang's AST directly in reflection
--- code and to provide a stable api for interacting with it so that when 
--- (or if) we update llvm we don't have to worry about fixing all reflection
--- code that uses it.
---
--- In the future we should experiment with caching this information and 
--- only generate it when whatever being parsed actually changes. We could 
--- possibly do this by intercepting includes/imports and doing this process
--- when we don't already have recently cached data
---

local lpp = require "lpp"
local ast = require "reflect.AST"
local util = require "util"
local List = require "list"
local log = require "logger" ("reflect.processor", Verbosity.Info)

require "lppclang" .init "../lppclang/build/debug/liblppclang.so"

local Processor = {}
Processor.__index = Processor

-- * --------------------------------------------------------------------------

Processor.new = function(input)
  local o = {}
  o.clang = require "reflect.ClangContext" ()
  o.clang:parseString(input)
  -- Map from the original clang object to our internal 
  -- representation of it.
  o.processed_clang_objs = {}
  o.record_stack = List{}
  o.top_level = List{}
  o.decls = 
  {
    list = List{},
    map = {},
  }
  o.prereqs = {}
  return setmetatable(o, Processor)
end

-- * --------------------------------------------------------------------------

Processor.recordDecl = function(self, decl)
  if self.decls.map[decl.name] then return end

  decl.decl_idx = #self.decls.list
  self.decls.map[decl.name] = decl
  self.decls.list:push{ name=decl.name, decl=decl }
end

-- * --------------------------------------------------------------------------

Processor.eachRecordDeclWithIndex = function(self)
  local iter = self.decls.list:eachWithIndex()
  return function()
    while true do
      local decl,i = iter()
      if not decl then
        return
      end
      if decl.decl:is(ast.Record) then
        return decl.name, decl.decl, i-1
      end
    end
  end
end

-- * --------------------------------------------------------------------------

Processor.recordProcessed = function(self, canonical_name, obj)
  self.processed_clang_objs[canonical_name] = obj
end

-- * --------------------------------------------------------------------------

Processor.findProcessed = function(self, canonical_name)
    return self.processed_clang_objs[canonical_name]
end

-- * --------------------------------------------------------------------------

Processor.pushRecord = function(self, record)
  self.record_stack:push(record)
end

-- * --------------------------------------------------------------------------

Processor.popRecord = function(self)
  return self.record_stack:pop()
end

-- * --------------------------------------------------------------------------

Processor.currentRecord = function(self)
  return self.record_stack:last()
end

-- * --------------------------------------------------------------------------

Processor.run = function(self)
  local tu = self.clang:getTranslationUnitDecl()
  local iter = tu:getDeclIter()
  while true do
    local cdecl = iter:next()
    if not cdecl then break end

    local decl = self:processTopLevelDecl(cdecl)
    if decl then

      -- We defer recording tag decls until we finish a top level decl to 
      -- ensure that cyclic dependencies, eg.
      --   
      --   struct Object;
      --
      --   struct Thing
      --   {
      --     Object* obj;
      --   };
      --
      --   struct Object
      --   {
      --     Thing thing;
      --   };
      --
      -- are ordered correctly. Since we eagerly resolve the declarations 
      -- of types of members, Object will be resolved and recorded before 
      -- Thing, and so placed before it in the decl list. Making sure the order
      -- is correct here makes it easier to generate code dependent on the 
      -- order in which things are declared being correct, eg. the Inspector.
      local function recordPrereqs(decl)
        if decl.prereqs then
          for prereq in decl.prereqs.list:each() do
            recordPrereqs(prereq)
          end
        end
        decl.prereqs = nil
        self:recordDecl(decl)
      end
      recordPrereqs(decl)

      self.top_level:push(decl)
    end
  end
end

-- * --------------------------------------------------------------------------

Processor.processTopLevelDecl = function(self, cdecl)
  if cdecl:name() == "" then return end

  if cdecl:isTemplateSpecialization() then return end

  -- if cdecl:name() == "GameMgr" or cdecl:name() == "Engine" then
  --   print(cdecl:name())
  -- end

  return (self:resolveDecl(cdecl))
end

-- * --------------------------------------------------------------------------

Processor.resolveDecl = function(self, cdecl)
  cdecl = self:ensureDefinitionDecl(cdecl)

  -- NOTE(sushi) we should only be getting type decls in here for now.
  local ctype = cdecl:getTypeDeclType()
  if not ctype then
    return
  end
  local canonical_name = ctype:getCanonicalTypeName()

  local processed = self:findProcessed(canonical_name)
  if processed then
    return processed
  end

  -- Ignore template declarations for now.
  if cdecl:isTemplate() then return end

  -- Make sure this type is complete.
  ctype:makeComplete()

  local decl

  if cdecl:isTemplateSpecialization() then
    decl = self:processTemplateSpecialization(cdecl, ctype)
  elseif cdecl:isStruct() then
    decl = self:processStruct(cdecl, ctype) 
  elseif cdecl:isUnion() then
    decl = self:processUnion(cdecl, ctype)
  elseif cdecl:isEnum() then
    decl = self:processEnum(cdecl, ctype)
  end

  -- if decl and 
  --    (decl.name == "struct GameMgr" or
  --     decl.name == "struct Engine")
  -- then
  --   decl:dump()
  -- end

  if decl then
    -- self:recordDecl(decl)
  end

  return decl
end

-- * --------------------------------------------------------------------------

--- Resolves the given ctype to our internal representation if it exists.
--- If it doesn't exist then process it and return the result.
Processor.resolveType = function(self, ctype)
  local type

  if ctype:isPointer() then
    if ctype:isFunctionPointer() then
      type = ast.FunctionPointerType.new()
    else
      local subctype = ctype:getPointeeType()
      local subtype = self:resolveType(subctype)
      type = ast.PointerType.new(subtype)
    end
  elseif ctype:isReference() then
    local subctype = ctype:getPointeeType()
    local subtype = self:resolveType(subctype)
    type = ast.ReferenceType.new(subtype)
  elseif ctype:isArray() then
    local subctype = ctype:getArrayElementType()
    local subtype = self:resolveType(subctype)
    local len = ctype:getArrayLen()
    type = ast.CArray.new(subtype, len)
  elseif ctype:isElaborated() then
    type = 
      ast.ElaboratedType.new(
        ctype:getTypeName(),
        self:resolveType(ctype:getDesugaredType()))
  elseif ctype:isBuiltin() then
    type = 
      ast.BuiltinType.new(
        ctype:getCanonicalTypeName(), 
        ctype:size() / 8)
  else
    local builtin = ast.BuiltinType[ctype:getTypeName()]
    if builtin then
      return builtin
    end

    local cdecl = ctype:getDecl()
    if not cdecl then
      ctype:dump()
      error("failed to get declaration of type '"..
            ctype:getCanonicalTypeName().."'")
    end

    local decl = self:resolveDecl(cdecl)
    if not decl then
      error("unable to resolve decl of type '"..ctype:getCanonicalTypeName()..
            "'")
    end

    if decl:is(ast.TagDecl) then
      type = ast.TagType.new(ctype:getTypeName(), decl)
    else
      error("unhandled decl kind of type '"..ctype:getCanonicalTypeName()..
            "'")
    end
  end

  return type
end

-- * --------------------------------------------------------------------------

Processor.ensureDefinitionDecl = function(self, cdecl)
  -- Ensure we are dealing with the actual definition of this decl
  local defdecl = cdecl:getDefinition()
  if defdecl then
    return defdecl
  else
    return cdecl
  end
end

-- * --------------------------------------------------------------------------

Processor.processStruct = function(self, cdecl, ctype)
  local struct = ast.Struct.new(ctype:getCanonicalTypeName())
  self:recordProcessed(ctype:getCanonicalTypeName(), struct)

  self:processRecordMembers(cdecl, ctype, struct)

  return struct
end

-- * --------------------------------------------------------------------------

Processor.processUnion = function(self, cdecl, ctype)
  local union = ast.Union.new(ctype:getCanonicalTypeName())
  self:recordProcessed(ctype:getCanonicalTypeName(), union)

  self:processRecordMembers(cdecl, ctype, union)

  return union
end

-- * --------------------------------------------------------------------------

Processor.processTemplateSpecialization = function(self, cdecl, ctype)
  local specdecl = cdecl:getSpecializedDecl()
  local spec = 
    ast.TemplateSpecialization.new(
      ctype:getCanonicalTypeName(),
      specdecl:name())

  self:recordProcessed(spec.name, spec)

  local iter = cdecl:getTemplateArgIter()
  while true do
    local arg = iter:next()
    if not arg then break end

    if arg:isType() then
      spec.template_args:push(self:resolveType(arg:getAsType()))
    elseif arg:isIntegral() then
      spec.template_args:push(arg:getAsIntegral())
    else
      log:warn("unhandled template arg kind in ", spec.name, "\n")
    end
  end

  self:processRecordMembers(cdecl, ctype, spec)

  return spec
end

-- * --------------------------------------------------------------------------

Processor.processRecordMembers = function(self, cdecl, ctype, record)
  local member_iter = cdecl:getDeclIter()

  record.prereqs = 
  {
    map = {},
    list = List{},
  }

  local function recordPrereq(decl) 
    if record.prereqs.map[decl] then
      return
    end

    record.prereqs.map[decl] = true
    record.prereqs.list:push(decl)
  end

  while member_iter do
    local member_cdecl = member_iter:next()
    if not member_cdecl then
      break
    end
    
    if member_cdecl:isField() then
      local field_ctype = member_cdecl:type()
      local field_type = self:resolveType(field_ctype)

      local type = field_type
      while true do
        if type:is(ast.ElaboratedType) then
          type = type.desugared
        elseif type:is(ast.TagType) then
          recordPrereq(type.decl)
          break
        else
          break
        end
      end

      local field = 
        ast.Field.new(
          member_cdecl:name(),
          field_type,
          member_cdecl:getFieldOffset() / 8)

      record:addMember(member_cdecl:name(), field)
    elseif member_cdecl:isTagDecl() then
      local member_decl = self:resolveDecl(member_cdecl)
      member_decl.parent = record
      record:addMember(member_cdecl:name(), member_decl)
    end
  end
end

-- * --------------------------------------------------------------------------

Processor.processEnum = function(self, cdecl, ctype)
  local enum = ast.Enum.new(ctype:getCanonicalTypeName())
  self:recordProcessed(ctype:getCanonicalTypeName(), enum)

  local iter = cdecl:getEnumIter()
  if iter then
    while true do
      local elem = iter:next()
      if not elem then break end

      enum.elems:push(elem:name())
    end
  end

  return enum
end

return Processor
