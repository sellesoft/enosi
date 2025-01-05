-- Reflection 'system'.
local lpp = require "lpp"
local List = require "list"
local buffer = require "string.buffer"

-- Load lppclang.
-- TODO(sushi) the lib needs to be loaded in a better way.
require "ffi" .load "lppclang"
require "lppclang" .init "lppclang"


-- Get the cargs so we can pass them to lppclang when we create the context.
local args = List{}

local nix_cflags = os.getenv("NIX_CFLAGS_COMPILE")
if nix_cflags then
  for arg in nix_cflags:gmatch("%S+") do
    args:push(arg)
  end
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

Reflect.createContext = function()
  return lpp.clang.createContext(args)
end

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

-- TODO(sushi) this needs to be a part of lpp itself.
local import_list = List{}
lpp.import = function(path)
  local full_path = path
  if full_path:sub(1,1) ~= "/" then
    local src_path = full_path
    if not full_path:find("^src/") then
      full_path = "src/"..full_path
    end
    full_path = lpp.getFileFullPathIfExists(full_path)
  end
  if not full_path or full_path == "" then
    error("could not form full path to "..path)
  end

  if imported[full_path] then
    return 
  end

  import_list:push(path)

  imported[full_path] = true
  local result = lpp.processFile(full_path)

  import_list:pop()

  local buf = buffer.new()

  buf:put [[
// - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * 
// import chain:
]]

  for import in import_list:each() do
    buf:put("// ", import, "\n")
  end

  buf:put [[
// - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * 
]]

  buf:put("// ", path, ": \n\n")

  buf:put(result)

  local expansion = lpp.MacroExpansion.new()
  expansion:pushBack(
    lpp.MacroPart.new(
      full_path, 0, 0, tostring(buf)))

  return buf:get()
end

if lpp.generating_dep_file then
  lpp.registerFinal(function(result)
    local makedeps = 
      lpp.clang.generateMakeDepFile(
        lpp.getCurrentInputSourceName(), result, args)

    for f in makedeps:gmatch("%S+") do
      -- NOTE(sushi) filtering out unwanted stuff here. This will unfortunately
      --             break subtley if cpp files are ever included. But I 
      --             currently don't plan to do this anyways.
      -- TODO(sushi) write a custom dependency consumer thing so that 
      --             we dont have to do this and can instead directly
      --             output a newline delimited list.
      if f:sub(-1) == ":" or f == "\\" or f:find(".cpp$") or f:find("^/usr/") 
      then
        goto continue
      end

      lpp.addDependency(f)

      ::continue::
    end
  end)
end

return Reflect
