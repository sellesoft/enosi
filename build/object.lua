local Type = require "Type"
local List = require "list"
local helpers = require "build.helpers"

local object = {}

local Object = Type.make()
object.Object = Object

---@class object.CppObj
local CppObj = Object:derive()
object.CppObj = CppObj

--- A collection of parameters to alter how a cpp object file is built.
---
---@class object.CppObjParams
--- The C++ std to use.
---@field std string
--- The optimization to use. Default is 'none'.
--- May be one of:
---   none
---   size
---   speed
---@field opt string
--- List of preprocessor defines given in the form:
---   { name, value }
---@field defines List
--- If debug info should be emitted.
---@field debug_info boolean
--- Directories to search for includes.
---@field include_dirs List
--- Disable building with C++'s builtin RTTI.
---@field nortti boolean
--- Force all symbols exposed to the dynamic table by default.
--- Default is false.
---@field export_all boolean

CppObj.Cmd = Type.make()

---@param params object.CppObjParams
CppObj.Cmd.new = function(params, compiler)
  local o = {}
  o.compiler = compiler

  if compiler == "clang++" then
    o.partial = helpers.listBuilder(
      "clang++",
      "-c",
      CppObj.Cmd.getIOIndependentFlags(params, compiler))
  else
    error("unhandled compiler: "..compiler)
  end

  return setmetatable(o, CppObj.Cmd)
end

CppObj.Cmd.getIOIndependentFlags = function(params, compiler)
  local o
  if compiler == "clang++" then
    o = helpers.listBuilder(
      "-std="..(params.std or "c++20"),
      params.nortti and "-fno-rtti",
      params.debug_info and "-ggdb3",
      not params.export_all and "-fvisibility=hidden",
      "-fpatchable-function-entry=16",
      "-Wno-#warnings",
      "-fmessage-length=80",
      "-fdiagnostics-absolute-paths")

    o:push(helpers.switch(params.opt,
    {
      none = "-O0",
      size = "-Os",
      speed = "-O2",
    }) or error("invalid opt "..params.opt))

    for define in params.defines:each() do
      if define[2] then
        o:push("-D"..define[1].."="..define[2])
      else
        o:push("-D"..define[1])
      end
    end

    for include_dir in params.include_dirs:each() do
      o:push("-I"..include_dir)
    end
  else
    error("unhandled compiler "..compiler)
  end
  return o
end

CppObj.Cmd.complete = function(self, cfile, ofile)
  if "clang++" == self.compiler then
    return helpers.listBuilder(
      self.partial,
      cfile,
      "-o",
      ofile)
  end
end

---@param src string 
---@param dst string 
---@param cmd object.CppObj.Cmd
---@return object.CppObj
CppObj.new = function(src, dst, cmd)
  local o = {}
  o.src = src
  o.dst = dst
  o.params = params
  return setmetatable(o, CppObj)
end

CppObj.getRecipe = function(self, compiler)
  return function()
    
  end
end

return object




