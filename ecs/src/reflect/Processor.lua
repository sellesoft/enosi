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

local ast = require "reflect.AST"
local List = require "List"
local log = require "Logger" ("reflect.processor", Verbosity.Info)
local metadata = require "reflect.Metadata"

require "lppclang" .init "../lppclang/build/debug/liblppclang.so"

local Processor = {}
Processor.__index = Processor

-- * --------------------------------------------------------------------------

Processor.new = function(input, filter)
  local o = {}
  o.filter = filter
  o.clang = require "reflect.ClangContext" ()
  o.clang:parseString(input)
  -- Map from the original clang object to our internal
  -- representation of it.
  o.processed_clang_objs = {}
  o.processed_types = {}
  o.record_stack = List{}
  o.top_level = List{}
  o.decls =
  {
    list = List{},
    map = {},
  }
  o.types = 
  {
    list = List{},
    map = {},
  }
  o.prereqs = {}
  o.namespace_stack = List{}
  o.all_decls = List{}
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

Processor.recordProcessed = function(self, cdecl, obj)
  self.processed_clang_objs[tostring(cdecl.handle)] = obj
end

-- * --------------------------------------------------------------------------

Processor.findProcessed = function(self, cdecl)
  return self.processed_clang_objs[tostring(cdecl.handle)]
end

-- * --------------------------------------------------------------------------

Processor.lookupDecl = function(self, name)
  for _,v in pairs(self.processed_clang_objs) do
    if name == v.name then
      return v
    end
  end
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
  self:processTopLevelDecls(iter)

  for type in self.types.list:each() do
    local decl = type:getDesugared():getDecl()
    if decl then
      decl.types = decl.types or {}
      if type:is(ast.ElaboratedType) or type:is(ast.TagType) then
        decl.types[type.name] = type
      end
    end
  end
end

-- * --------------------------------------------------------------------------

Processor.processTopLevelDecls = function(self, iter)
  while true do
    local cdecl = iter:next()
    if not cdecl then break end

    local decl = self:processTopLevelDecl(cdecl)
    
    if decl then
      decl.namespace = self.namespace_stack:last()
    end

    if decl and (not self.filter or self:filter(decl)) then
      -- TODO(sushi) implement something that takes all the declarations 
      --             we've gathered and sorts them into some list that 
      --             covers all declarations (eg. those at top-level
      --             and those used nested in structs and all of that).
      --             We do a lot of dependency handling outside of here 
      --             which gets quite annoying and is very easy to mess up
      --             (and probably not the easiest thing to follow either).
      --             We apparently track 'prereqs' of declarations (as 
      --             used below) so it might not be that difficult to get
      --             something like this working. I think that its just set
      --             up currently to make sure that the top level decls 
      --             we track are in the proper order, not everything
      --             inside of them.

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

  if cdecl:isNamespace() then
    local name = cdecl:name()
    if name ~= "std" then
      local ns = ast.Namespace.new(name, self.namespace_stack:last())
      self.namespace_stack:push(ns)
      local iter = cdecl:getDeclIter()
      if iter then
        self:processTopLevelDecls(iter)
      end
      self.namespace_stack:pop()
    end
  else
    return (self:resolveDecl(cdecl))
  end
end

-- * --------------------------------------------------------------------------

Processor.resolveDecl = function(self, cdecl)
  cdecl = self:ensureDefinitionDecl(cdecl)

  local processed = self:findProcessed(cdecl)
  if processed then
    return processed
  end

  -- NOTE(sushi) we should only be getting type decls in here for now.
  local ctype = cdecl:getTypeDeclType()
  if not ctype then
    return
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
  elseif cdecl:isTypedef() then
    decl = self:processTypedef(cdecl, ctype)
  else
    cdecl:dump()
    error "unhandled decl"
  end

  if decl then
    decl.type = self:resolveType(ctype)
    decl.comment = cdecl:getComment()
    if decl:is(ast.Record) and cdecl:isAnonymous() then
      decl.is_anonymous = true
    end
    decl.typedefs = List{}
    decl.local_name = cdecl:name()

    self.all_decls:push(decl)
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
      type.size = ctype:size() / 8
    else
      local subctype = ctype:getPointeeType()
      local subtype = self:resolveType(subctype)
      type = ast.PointerType.new(subtype)
      type.size = ctype:size() / 8
    end
  elseif ctype:isReference() then
    local subctype = ctype:getPointeeType()
    local subtype = self:resolveType(subctype)
    type = ast.ReferenceType.new(subtype)
    type.size = ctype:size() / 8
  elseif ctype:isArray() then
    local subctype = ctype:getArrayElementType()
    local subtype = self:resolveType(subctype)
    local len = ctype:getArrayLen()
    type = ast.CArray.new(subtype, len)
    type.size = ctype:size() / 8
  elseif ctype:isElaborated() then
    type =
      ast.ElaboratedType.new(
        ctype:getTypeName(),
        self:resolveType(ctype:getSingleStepDesugaredType()))
    type.size = type.desugared.size
  elseif ctype:isBuiltin() then
    type =
      ast.BuiltinType.new(
        ctype:getCanonicalTypeName(),
        ctype:size() / 8)
  elseif ctype:isTypedef() then
    local decl = self:resolveDecl(ctype:getTypedefDecl())
    if not decl then
      ctype:dump()
      ctype:getTypedefDecl():dump()
    end
    type = ast.TypedefType.new(decl)
    if decl:is(ast.TypedefDecl) then
      type.size = decl.subtype.size
    else
      type.size = ctype:size() / 8
    end
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
      if cdecl:isComplete() then
        type.size = ctype:size() / 8
      end
    elseif decl:is(ast.TypedefDecl) then
      type = ast.TypedefType.new(decl)
      if cdecl:isComplete() then
        type.size = ctype:size() / 8
      end
    else
      require "Util" .dumpValue(getmetatable(decl), 2)
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

Processor.collectBaseAndDerived = function(self, cdecl, record)
  local bases = cdecl:getBaseIter()
  if bases then
    local base = bases:next()
    while base do
      local basedecl = base:getDecl()
      if basedecl then
        local decl = self:resolveDecl(basedecl)
        if decl then
          decl.derived:push(record)
          record.bases:push(decl)
          if record.prereqs and not record.prereqs.map[decl] then
            record.prereqs.map[decl] = true
            record.prereqs.list:push(decl)
          end
        end
      end
      base = bases:next()
    end
  end
end

-- * --------------------------------------------------------------------------

Processor.processStruct = function(self, cdecl, ctype)
  local struct = ast.Struct.new(ctype:getCanonicalTypeName())
  self:recordProcessed(cdecl, struct)

  self:processRecordMembers(cdecl, ctype, struct)

  cdecl = self:ensureDefinitionDecl(cdecl)

  if cdecl:isComplete() then
    self:collectBaseAndDerived(cdecl, struct)

    struct.comment = cdecl:getComment()
    if struct.comment then
      struct.metadata = metadata.__parse(struct.comment)
    end

    struct.is_complete = true
  else
    struct.is_complete = false
  end

  return struct
end

-- * --------------------------------------------------------------------------

Processor.processUnion = function(self, cdecl, ctype)
  local union = ast.Union.new(ctype:getCanonicalTypeName())
  self:recordProcessed(cdecl, union)

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

  self:recordProcessed(cdecl, spec)

  local iter = cdecl:getTemplateArgIter()
  while true do
    local arg = iter:next()
    if not arg then break end

    if arg:isType() then
      spec.template_args:push(self:resolveType(arg:getAsType()))
    elseif arg:isIntegral() then
      spec.template_args:push(arg:getAsIntegral())
    else
      spec.has_unhandled_arg_kind = true
      log:warn("unhandled template arg kind in ", spec.name, "\n")
    end
  end

  if cdecl:isComplete() then
    self:collectBaseAndDerived(cdecl, spec)

    spec.comment = cdecl:getComment()
    if spec.comment then
      spec.metadata = metadata.__parse(spec.comment)
    end

    spec.is_complete = true
  end

  self:processRecordMembers(cdecl, ctype, spec)

  for arg in spec.template_args:each() do
    if type(arg) == "table" then
      local argdecl = arg:getDecl()
      if argdecl then
        if argdecl.has_unhandled_arg_kind then
          spec.has_unhandled_arg_kind = argdecl.has_unhandled_arg_kind
        end
        if not spec.prereqs.map[argdecl] then
          spec.prereqs.map[argdecl] = true
          spec.prereqs.list:push(argdecl)
        end 
      end
    end
  end

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

      if member_cdecl:isAnonymousField() then
        print("is_anonymous")
      end

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

      local field_cdecl = field_ctype:getDecl()

      if field_cdecl and field_cdecl:isAnonymous() then
        field_type:getDecl().is_anonymous = true
      end

      field.comment = member_cdecl:getComment()

      if field.comment then
        field.metadata = metadata.__parse(field.comment)
      else
        field.metadata = {}
      end

      record:addMember(member_cdecl:name(), field)
    elseif member_cdecl:isTagDecl() then
      local member_decl = self:resolveDecl(member_cdecl)
      member_decl.parent = record
      member_decl.has_unhandled_arg_kind = record.has_unhandled_arg_kind
      record:addMember(member_cdecl:name(), member_decl)
    elseif member_cdecl:isFunction() then
      local name = member_cdecl:name()
      record:addMember(name, ast.Function.new(name))
    end
  end
end

-- * --------------------------------------------------------------------------

Processor.processEnum = function(self, cdecl, ctype)
  local enum = ast.Enum.new(ctype:getCanonicalTypeName())
  self:recordProcessed(cdecl, enum)

  local iter = cdecl:getEnumIter()
  if iter then
    while true do
      local elem = iter:next()
      if not elem then break end

      local e = {}
      e.name = elem:name()
      e.comment = elem:getComment()
      if e.comment then
        e.metadata = metadata.__parse(e.comment)
      else
        e.metadata = {}
      end

      e.value = elem:getEnumValue()

      enum.elems:push(e)
    end
  end

  return enum
end

-- * --------------------------------------------------------------------------

Processor.processTypedef = function(self, cdecl, ctype)
  local typedef = ast.TypedefDecl.new(ctype:getTypeName())
  typedef.subtype = self:resolveType(cdecl:getTypedefSubType())
    or error("failed to get subtype of "..ctype:getTypeName())
  self:recordProcessed(cdecl, typedef)
  
  typedef.comment = cdecl:getComment()
  if typedef.comment then
    typedef.metadata = metadata.__parse(typedef.comment)
  end

  local subdecl = typedef.subtype:getDecl()
  if subdecl then
    subdecl.typedefs:push(typedef)
  end

  return typedef
end

return Processor
