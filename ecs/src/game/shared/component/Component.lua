local ast = require "reflect.AST"

local comp = {}

comp.importComponents = function()
  return require "reflect.Importer"
  {
    patterns = { "src/game/**/*.comp.lh" },

    filter = function(p, decl)
      local TComponent = p.processed_clang_objs["struct Component"]
      if TComponent and decl:is(ast.Record) and decl:isDerivedFrom(TComponent)
      then
        return true
      end
    end
  }
end

return comp
