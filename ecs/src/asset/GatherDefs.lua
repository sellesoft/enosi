local cmn = require "common"
local glob = require "Glob"
local Processor = require "reflect.Processor"
local Type = require "Type"
local ast = require "reflect.AST"

local defs = {}

local Defs = Type.make()

defs.gather = function()
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

  local p = Processor.new(tostring(imported))
  p:run()

  local o = {}
  o.p = p
  o.imported = imported
  return setmetatable(o, Defs)
end

local function isExplicitlyFiltered(name)
  name = defs.removeTag(name)

  -- TODO(sushi) really need to set up the Processor to take in a 
  --             filter or something that lets us only gather definitions 
  --             from types that appear in the files we're interested in
  --             and those types that they use.
  if name:find "^iro::Log" or 
     name:find "^iro::io" or
     name:find "^iro::color" or
     name:find "^iro::fs" or
     name:find "^iro::mem" or
     name:find "^iro::Process" or
     name:find "^iro::platform" or
     name == "iro::TimeSpan" or
     name == "iro::TimePoint" or
     name == "iro::UTCDate" or
     name == "iro::LocalDate" or
     name == "iro::WithUnits" or
     name == "iro::ScopedIndent" or 
     name == "defer_dummy" or 
     name == "defer_with_cancel_dummy" or
     name == "Nil" or
     name == "gfx::Renderer" or 
     name == "OffsetPtr" or 
     name == "OffsetString" or 
     name == "OffsetSlice"
  then
    return true
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
          if decl.specialized_name == "Array" then
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

