---
--- Helper for importing files based on a pattern, processing those files
--- into our internal AST types and filtering the declarations retrieved from
--- the Processor.
---
--- The filter is used to decide what top level declarations are 'recorded'
--- by the Processor. When a declaration is recorded, all of the things
--- it uses are too. This way, we can record only declarations we are
--- interested in as well as what they use. This avoids a lot of structures we
--- generally don't care about, eg. a lot of iro structures like iro::Log,
--- defer_dummy, iro::Process, etc.
---

local cmn = require "common"
local glob = require "Glob"
local Processor = require "reflect.Processor"
local ast = require "reflect.AST"
local reflect = require "reflect.Reflector"

local Importer = require "Type" .make()

Importer.new = function(opts)
  local o = {}
  o.filter = opts.filter
  o.patterns = cmn.List(opts.patterns)
  o.excludes = cmn.List(opts.excludes)

  if opts.suppress_imports then
    -- NOTE(sushi) i have no idea what this was for so be wary.
    print "suppress_imports"
    reflect.pushImportedStack()
  end

  ECS_REFLECTION_IMPORT = true

  o.imported = cmn.buffer.new()
  for pattern in o.patterns:each() do
    glob(pattern):each(function(path)
      local excluded = false
      for exclude in o.excludes:each() do
        if exclude == path then
          excluded = true
          break
        end
      end

      if not excluded then
        o.imported:put(lpp.import(path))
      end
    end)
  end

  ECS_REFLECTION_IMPORT = nil

  if opts.suppress_imports then
    reflect.popImportedStack()
  end

  o.p = Processor.new(tostring(o.imported), o.filter)
  o.p:run()

  return setmetatable(o, Importer)
end

Importer.eachDecl = function(self, f)
  for decl in self.p.decls.list:each() do
    f(Importer.removeTag(decl.name), decl.decl)
  end
end

local function formTypeName(name, type)
  if type:is(ast.ElaboratedType) then
    name:put(type.name)
  elseif type:is(ast.TagType) then
    name:put(type.name)
  elseif type:is(ast.BuiltinType) then
    name:put(type.name)
  elseif type:is(ast.PointerType) then
    formTypeName(name, type.subtype)
    name:put "*"
  elseif type:is(ast.ReferenceType) then
    formTypeName(name, type.subtype)
    name:put "&"
  elseif type:is(ast.CArray) then
    formTypeName(name, type.subtype)
    name:put('[', type.len, ']')
  else
    io.write("unhandled type kind: ", type:tostring(), "\n")
  end
end

Importer.eachType = function(self, f)
  for type in self.p.types.list:each() do
    local name = cmn.buffer.new()
    formTypeName(name, type)
    f(name:get(), type)
  end
end

Importer.removeTag = function(name)
  return name
    :gsub("struct ", "")
    :gsub("union ", "")
    :gsub("enum ", "")
end

Importer.get = function(self)
  return self.imported:get()
end

return function(opts)
  return Importer.new(opts)
end
