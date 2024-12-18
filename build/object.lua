--- 
--- Internal representations of 'build objects', which are anything that is
--- built and/or used to build other build objects.
---

local util = require "util"
local sys = require "build.sys"
local Type = require "Type"
local List = require "list"
local helpers = require "build.helpers"
local cmd = require "build.commands"
local buffer = require "string.buffer"

local build = 
{
  sys = require "build.sys",
  cmd = require "build.commands",
}

local object = {}

--- The base of all build objects.
---@class object.BuildObject : iro.Type
--- The directory which this build object comes from, eg. the project's 
--- root.
---@field mount string
--- Set when this build object is not intended to be built as its expected 
--- to come from elsewhere. This is mostly useful when dealing with things 
--- that are built via a makefile or cmake. This just prevents the driver
--- from creating a lake command to build this object.
---@field no_build boolean
local BuildObject = Type.make()
object.BuildObject = BuildObject

--- Indicates if this build object can be built using some command. We assume
--- most will be, but some objects, such as External objects, do not need 
--- to be built.
BuildObject.hasCmd = function()
  return true
end

BuildObject.__tostring = function(self)
  return "BuildObject("..self.bobj_name..")"
end

BuildObject.assignMount = function(self, mount)
  if not self.mount then
    self.mount = mount
  end
end

BuildObject.formAbsolutePath = function(self)
  return self.mount.."/"..self:getTargetName()
end

object.BuildObject.list = List{}

setmetatable(object,
{
  __newindex = function(_, k, v)
    rawset(object, k, v)

    getmetatable(v).__tostring = function()
      return k
    end

    v.bobj_name = k

    object.BuildObject.list:push(v)

    -- Automatically set the Cmd subtype on objects based on name, if they 
    -- can be built.

    -- if v.hasCmd() then
    --   v.Cmd = cmd[k]
    --   if not v.Cmd then
    --     error(
    --       "no corresponding Cmd could be found for build object "..k.."\n"..
    --       "  an associated Cmd must exist for all build objects defined in\n"..
    --       "  build/objects.lua (eg. one defined in build/commands.lua)\n")
    --   end
    -- end
  end
})

--- An object file of some kind. This class is not meant to be used directly,
--- instead a derived type, such as CppObj, should be used to indicate where
--- the obj file comes from (so we know how to build it). This class should
--- be used to generically determine if a build object is an obj file, eg.
---
---  if bobj.is(object.Obj) then
---    ...
---  end
---
---@class object.Obj : object.BuildObject
--- The file this obj will be built from.
---@field src string
local Obj = BuildObject:derive()

--- Gets the full target name of this obj file.
Obj.getTargetName = function(self)
  return self.src..".o"
end

--- An obj file built from a cpp file.
---@class object.CppObj : object.Obj
--- The cpp file we are building it from.
---@field src string
local CppObj = Obj:derive()
object.CppObj = CppObj

---@param src string 
---@return object.CppObj
CppObj.new = function(src)
  local o = {}
  o.src = src
  return setmetatable(o, CppObj)
end

---@param cmd cmd.CppObj
CppObj.makeTask = function(self, cmd, proj)
  local cfile = proj.wdir.."/"..self.src
  local ofile = proj:formOutputPath(self.src..".o")
  local dfile = proj:formOutputPath(self.src..".d")
  local cpp_cmd = cmd:complete(cfile, ofile)

  lake.mkdir(ofile:match("(.*)/.*"), { make_parents=true })

  ---@type cmd.CppDeps.Params
  local dep_params = {}

  dep_params.compiler = cmd.params.compiler
  dep_params.include_dirs = cmd.params.include_dirs
  dep_params.defines = cmd.params.defines

  local dep_cmd = build.cmd.CppDeps.new(dep_params):complete(self.src)

  -- TODO(sushi) there doesn't need to be a cpp task, we can just check it 
  --             manually in the obj/dep conds
  local cpp_task = lake.task(cfile)
  local obj_task = lake.task(ofile)
  local dep_task = lake.task(dfile)

  obj_task:dependsOn(cpp_task):workingDirectory(proj.wdir)
  dep_task:dependsOn(cpp_task):workingDirectory(proj.wdir)

  local depfile = io.open(dfile, "r")
  if depfile then
    for file in depfile:lines() do
      obj_task:dependsOn(lake.task(file))
    end
  end

  local docond = 
    function(task)
      lake.mkdir(task.name:match("(.*)/"))
      return function(prereqs)
        if not lake.pathExists(task.name) then
          io.write(task.name, " does not exist\n")
          return true
        end

        local target_modtime = lake.modtime(task.name)
        for prereq in prereqs:each() do
          if lake.modtime(prereq) > target_modtime then
            io.write(prereq, " is newer than ", task.name, "\n")
            return true
          end
        end
      end
    end

  obj_task:cond(docond(obj_task))
    :recipe(function()
      local output = buffer.new()
      local result = lake.cmd(cpp_cmd,
      {
        onRead = function(x) output:put(x) end
      })
      if result ~= 0 then
        io.write("failed to compile ", ofile, ":\n", output:get(), "\n")
      else
        io.write(
          "compiled ", helpers.makeRootRelativePath(sys.root, ofile), "\n")
      end
    end)

  dep_task:cond(docond(dep_task))
    :recipe(function()
      local output = buffer.new()
      local result = lake.cmd(dep_cmd, 
        { 
          onRead = function(s)
            output:put(s)
          end
        })
      
      local depfile = io.open(dfile, "w")
      if not depfile then
        error("failed to open depfile at "..dfile)
      end
    
      for f in output:get():gmatch("%S+") do
        if f:sub(-1) ~= ":" and 
           f ~= "\\" 
        then
          local fullpath = f
          if f:sub(1,1) ~= "/" then
            fullpath = sys.root.."/"..f
          end
          fullpath = fullpath:gsub("/[^/]-/%.%.", "")
          depfile:write(fullpath, "\n")
        end
      end

      depfile:close()
    end)
end

--- An obj file built from a lua file.
---@class object.LuaObj : object.Obj
local LuaObj = Obj:derive()
object.LuaObj = LuaObj

---@param src string
---@return object.LuaObj
LuaObj.new = function(src)
  local o = {}
  o.src = src
  return setmetatable(o, LuaObj)
end

LuaObj.makeTask = function(self, cmd, proj)
  local out = proj:formOutputPath(self.src..".o")
  lake.mkdir(helpers.getPathDirname(out), { make_parents = true })
  local comp = cmd:complete(self.src, out)

  lake.task(out)
    :cond(function()
      if not lake.pathExists(out) then
        return true
      end

      if lake.modtime(self.src) > lake.modtime(out) then
        return true
      end
    end)
    :recipe(function()
      local output = buffer.new()
      local result = lake.cmd(comp,
      {
        onRead = function(x) output:put(x) end
      })
      if result ~= 0 then
        io.write("failed to compile ", out, ":\n", output:get(), "\n")
      else
        io.write(
          "compiled ", helpers.makeRootRelativePath(sys.root, out), "\n")
      end
    end)
    :workingDirectory(proj.wdir)
end

--- An executable built from some collection of object files.
---@class object.Exe
--- The name of the executable.
---@field name string
--- The object files used to build the executable.
---@field objs List
local Exe = BuildObject:derive()
object.Exe = Exe

Exe.new = function(name, objs)
  local o = {}
  o.name = name
  o.objs = objs
  return setmetatable(o, Exe)
end

Exe.getTargetName = function(self)
  return self.name
end

---@param cmd cmd.Exe
Exe.makeTask = function(self, cmd, proj)
  local out = proj:formOutputPath(self.name)

  local objs = List{}
  for obj in self.objs:each() do
    objs:push(obj:formAbsolutePath())
  end

  local exe_cmd = cmd:complete(objs, out)

  local fin = ""
  for part in lake.flatten(exe_cmd):each() do
    fin = fin.."\n"..part
  end 
  print(fin)

  lake.task(out)
    :dependsOn(self.objs)
    :workingDirectory(proj.wdir)
    :cond(function(prereqs)
      if not lake.pathExists(out) then
        return true
      end

      local exe_modtime = lake.modtime(out)
      for prereq in prereqs:each() do
        if lake.modtime(prereq) > exe_modtime then
          return true
        end
      end
    end)
    :recipe(function()
      local output = buffer.new()
      local result = lake.cmd(exe_cmd,
      {
        onRead = function(x) output:put(x) end
      })
      if result ~= 0 then
        io.write("failed to compile ", out, ":\n", output:get(), "\n")
      else
        io.write(
          "compiled ", helpers.makeRootRelativePath(sys.root, out), "\n")
      end
    end)
end

--- A static library.
--- @class object.StaticLib : object.BuildObject
local StaticLib = BuildObject:derive()
object.StaticLib = StaticLib

StaticLib.new = function(name, objs)
  local o = {}
  o.name = name
  o.objs = objs
  return setmetatable(o, StaticLib)
end

StaticLib.getTargetName = function(self)
  local name = self.name 

  local dir, base = name:match "(.*)/(.*)"
  if not dir then
    base = name
  end

  if sys.os == "linux" then
    if dir then
      return dir.."/lib"..base..".a"
    else
      return "lib"..base..".a"
    end
  else
    error("unhandled os "..sys.os)
  end
end

local SharedLib = BuildObject:derive()
object.SharedLib = SharedLib

SharedLib.new = function(name, objs)
  local o = {}
  o.name = name
  o.objs = objs
  return setmetatable(o, SharedLib)
end

SharedLib.getTargetName = function(self)
  local dir,base = self.name:match "(.*)/(.*)"
  if not dir then
    base = self.name
  end
    
  if sys.os == "linux" then
    if dir then
      return dir.."/lib"..base..".so"
    else
      return "lib"..base..".so"
    end
  else
    error("unhandled os "..sys.os)
  end
end

--- A build object that is expected to exist on the system.
---@class object.External : object.BuildObject
local External = BuildObject:derive()

External.hasCmd = function()
  return false
end

object.External = External

---@class object.ExternalSharedLib : object.External
local ExternalSharedLib = External:derive()
object.ExternalSharedLib = ExternalSharedLib

---@param name string
---@return object.ExternalSharedLib
ExternalSharedLib.new = function(name)
  local o = {}
  o.name = name
  return setmetatable(o, ExternalSharedLib)
end

--- A special build object that specifies a makefile that needs to be run.
--- This should only be used for compiling external libraries that use 
--- make for compilation. 
---@class object.Makefile : object.BuildObject
local Makefile = BuildObject:derive()
object.Makefile = Makefile

--- The directory make should run in.
---@param wdir string
Makefile.new = function(wdir)
  local o = {}
  o.wdir = wdir
  return setmetatable(o, Makefile)
end

return object




