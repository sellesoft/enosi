--- 
--- Collection of commands that are used to build build objects.
---
--- Some Cmds take a table of parameters on creation and create an object
--- storing a command template to be filled in later via a call to 'complete'.
--- This allows us to build the static parts of a command once, to be filled
--- out with variable args like input and output files later.
---
--- This file is intended to be completely independent of the build system
--- as a whole, eg. it should never depend on any of its state. Do not 
--- require "build.sys" here, nor require anything else that might.
---
--- The reason being that this file is also used by the repo initialization
--- script, where the build system is not active yet because lake will have 
--- yet to be built.
---

local Type = require "Type"
local helpers = require "build.helpers"

local cmd = {}

---@class cmd.CppObj
--- Partially built command.
---@field partial List
--- The compiler this command uses.
---@field compiler string
cmd.CppObj = Type.make()

---@class cmd.CppObj.Params
--- The compiler to use.
---@field compiler string
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

---@param params cmd.CppObj.Params
---@return cmd.CppObj
cmd.CppObj.new = function(params)
  local o = {}
  o.compiler = params.compiler

  if o.compiler == "clang++" then
    o.partial = helpers.listBuilder(
      "clang++",
      "-c",
      cmd.CppObj.getIOIndependentFlags(params))
  else
    error("unhandled compiler: "..o.compiler)
  end

  return setmetatable(o, cmd.CppObj)
end

cmd.CppObj.getIOIndependentFlags = function(params)
  local o
  if params.compiler == "clang++" then
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
    error("unhandled compiler "..params.compiler)
  end
  return o
end

cmd.CppObj.complete = function(self, cfile, ofile)
  if "clang++" == self.compiler then
    return helpers.listBuilder(
      self.partial,
      cfile,
      "-o",
      ofile)
  end
end

---@class cmd.LuaObj
--- Partially built command.
---@field partial List
cmd.LuaObj = Type.make()

---@class cmd.LuaObj.Params
--- Whether to keep debug info.
---@field debug_info boolean

---@param params cmd.LuaObj.Params
cmd.LuaObj.new = function(params)
  local o = {}

  params.debug_info = params.debug_info or true

  o.partial = helpers.listBuilder(
    "luajit", 
    "-b",
    params.debug_info and "-g")

  return setmetatable(o, cmd.LuaObj)
end

cmd.LuaObj.complete = function(self, lfile, ofile)
  return helpers.listBuilder(
    self.partial,
    lfile,
    ofile)
end

---@class cmd.Exe
---@field partial List
cmd.Exe = Type:make()

---@class cmd.Exe.Params
--- The linker to use.
---@field linker string
--- Libraries to link against. Currently these are wrapped in a group on 
--- Linux as I am too lazy to figure out what the proper linking order is
--- for LLVM. This will probably be fixed once I need to get LLVM building on 
--- Windows.
---@field shared_libs List
--- Static libs to link against. This is primarily useful when a library 
--- outputs both static and shared libs under the same name and the 
--- static lib is preferred.
---@field static_libs List
--- List of directories to search for libs in.
---@field libdirs List

---@param params cmd.Exe.Params
---@return cmd.Exe
cmd.Exe.new = function(params)
  local o = {}

  if "ld" == params.linker then
    o.partial = helpers.listBuilder(
      "ld",
      lake.flatten(params.libdirs):map(function(dir)
        return "-L"..dir
      end),
      "--start-group",
      lake.flatten(params.shared_libs):each(function(lib)
        return "-l"..lib
      end),
      lake.flatten(params.static_libs):each(function(lib)
        return "-l:lib"..lib..".a"
      end),
      "--end-group",
      "-E",
      -- Tell the exe's dynamic linker to check the directory its in 
      -- for shared libraries.
      "-rpath", "$ORIGIN")
  else
    error("unhandled linker "..params.linker)
  end

  return setmetatable(o, cmd.Exe)
end

cmd.Exe.complete = function(self, objs, out)
  return helpers.listBuilder(
    self.partial,
    objs,
    "-o",
    out)
end

---@class cmd.StaticLib.Params
--- The program to use.
---@field program string
--- The obj files.
---@field objs List
--- The name of the lib.
---@field name string

---@param params cmd.StaticLib.Params
cmd.StaticLib = function(params)
  if "ar" == params.program then
    return helpers.listBuilder(
      "ar",
      "rcs",
      "lib"..params.name..".a",
      params.objs)
  else
    error("unhandled program "..params.program)
  end
end

---@class cmd.Makefile
cmd.Makefile = Type.make()

---@class cmd.Makefile.Params
--- The directory make should be called in.
---@field wdir

return cmd
