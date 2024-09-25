-- Reflection 'system'.
local lpp = require "lpp"
local List = require "list"

-- Load lppclang.
-- TODO(sushi) the lib needs to be loaded in a better way.
require "lppclang" .init "../lppclang/build/debug/liblppclang.so"

-- Get the cargs so we can pass them to lppclang when we create the context.
local args = List{}

for arg in os.getenv("NIX_CFLAGS_COMPILE"):gmatch("%S+") do
  args:push(arg)
end

-- TODO(sushi) put this somewhere central lol this is dumb
string.startsWith = function(self, s)
  return self:sub(1, #s) == s
end

for _,v in ipairs(lpp.argv) do
  if v:startsWith "--cargs" then
    local cargs = v:sub(#"--cargs="+1)
    for carg in cargs:gmatch("[^,]+") do
      args:push(carg)
    end
  end
end

---@class Reflect
---@field ctx Ctx
local Reflect = {}

-- Create the clang context.
Reflect.ctx = lpp.clang.createContext(args)

-- Attempt at lazy loading the context. This didn't seem to change much 
-- but would ideally be helpful for files that don't use reflection at all
-- to avoid having to load clang.
--
-- Reflect.ctx = setmetatable({},
-- {
--   __index = function(self, key)
--     if Reflect.ctx == self then
--       Reflect.ctx = lpp.clang.createContext(args)
--     end
--     return Reflect.ctx[key]
--   end,
-- })

--- 'Includes' a C/C++ file. The file will be parsed by clang so that 
--- its definitions become available.
---
--- This should be used as a macro, for example:
---
---   @reflect.include "iro/time.h"
---
---@param path string
Reflect.include = function(path)
  local formed = '#include "'..path..'"'

  Reflect.ctx:parseString(formed)
  
  return formed
end

--- Imports a lpp file and parses it with clang so semantic
--- information may be retreived.
Reflect.import = function(path)
  local result = lpp.import(path)

  if result then
    Reflect.ctx:parseString(result)
  end

  return result
end

Reflect.beginNamespace = function(name)
  Reflect.ctx:beginNamespace(name)
  return "namespace "..name.."\n{\n"
end

Reflect.endNamespace = function()
  Reflect.ctx:endNamespace()
  return "}\n"
end

Reflect.lookupType = function(name)
  return Reflect.ctx:lookupType(name)
end

local imported = {}

lpp.import = function(path)
  if imported[path] then
    return 
  end

  imported[path] = true
  local result = lpp.processFile("src/"..path)

  local expansion = lpp.MacroExpansion.new()
  expansion:pushBack(
    lpp.MacroPart.new(
      path, 0, 0, result))

  return result
end

if lpp.generating_dep_file then
  lpp.registerFinal(function(result)
    local makedeps = 
      lpp.clang.generateMakeDepFile(result, args)

    local count = 0
    for f in makedeps:gmatch("%S+") do
      -- NOTE(sushi) skip the first two since they are they the phony 
      --             cpp file passed to clang and its obj file.
      -- TODO(sushi) write a custom dependency consumer thing so that 
      --             we dont have to do this and can instead directly
      --             output a newline delimited list.
      if count < 2 or f:sub(-1) == ":" or f == "\\" then
        count = count + 1
        goto continue
      end

      lpp.addDependency(f)

      ::continue::
    end
  end)
end

return Reflect
