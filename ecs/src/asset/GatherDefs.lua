local cmn = require "common"
local glob = require "Glob"
local Processor = require "reflect.Processor"
local Type = require "Type"
local ast = require "reflect.AST"

local defs = {}

local Defs = Type.make()

defs.gather = function(opts)
  local imported = cmn.buffer.new()

  local importPattern = function(pattern)
    glob(pattern):each(function(path)
      if path and path:find("%S") then
        imported:put(lpp.import(path))
      end
    end)
  end

  importPattern "src/**/*.defs.lh"
  importPattern "src/**/*.comp.lh"

  if opts and opts.extra_patterns then
    for p in opts.extra_patterns:each() do
      importPattern(p)
    end
  end

  local p = Processor.new(tostring(imported))
  p:run()

  local o = {}
  o.p = p
  o.imported = imported
  return setmetatable(o, Defs)
end

local function isExplicitlyFiltered(name)
  name = defs.removeTag(name)

  local filters =
  {
    "iro::Log",
    "iro::io",
    "iro::color",
    "iro::fs",
    "iro::Process",
    "iro::mem",
    "iro::platform",
    "iro::Time.*",
    "iro::UTCDate",
    "iro::LocalDate",
    "iro::WithUnits",
    "iro::ScopedIndent",
    "defer_dummy",
    "defer_with_cancel_dummy",
    "Nil",
    "gfx::Renderer",
    "OffsetPtr",
    "OffsetString",
    "OffsetSlice",
    "CompiledData",
  }

  -- TODO(sushi) really need to set up the Processor to take in a 
  --             filter or something that lets us only gather definitions 
  --             from types that appear in the files we're interested in
  --             and those types that they use.

  for _,filter in ipairs(filters) do
    if name:find(filter) then
      return true
    end
  end
end

defs.removeTag = function(name)
  return name
     :gsub("struct ", "")
     :gsub("union ", "")
     :gsub("enum ", "")
end

Defs.eachDecl = function(self, f)
  for decl in self.p.decls.list:each() do
    local can_emit = false
    decl = decl.decl
    if decl then
      local name = defs.removeTag(decl.name)
      if not isExplicitlyFiltered(name) then
        if decl:is(ast.TemplateSpecialization) then
          if decl.specialized_name == "ArrayDef" then
            local subtype = decl.template_args[1]
            local subdecl = subtype:getDecl()
            if subdecl then
              if not isExplicitlyFiltered(subdecl.name) then
                can_emit = true
              end
            else
              if subtype:is(ast.PointerType) then
                can_emit = true
              end
            end
          end
        elseif decl:is(ast.Struct) or decl:is(ast.Enum) then
          can_emit = true
        end
      end
    end
    
    if can_emit then
      f(defs.removeTag(decl.name), decl)
    end
  end
end

return defs

