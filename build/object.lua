--- 
--- Internal representations of 'build objects', which are anything that is
--- built and/or used to build other build objects.
---

local sys = require "build.sys"
local Type = require "Type"
local List = require "list"
local helpers = require "build.helpers"
local cmd = require "build.commands"

local object = {}

--- The base of all build objects.
---@class object.BuildObject : iro.Type
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

    if v.hasCmd() then
      v.Cmd = cmd[k]
      if not v.Cmd then
        error(
          "no corresponding Cmd could be found for build object "..k.."\n"..
          "  an associated Cmd must exist for all build objects defined in\n"..
          "  build/objects.lua (eg. one defined in build/commands.lua)\n")
      end
    end
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
local Obj = BuildObject:derive()

--- An obj file built from a cpp file.
---@class object.CppObj : object.Obj
local CppObj = Obj:derive()
object.CppObj = CppObj

---@param src string 
---@return object.CppObj
CppObj.new = function(src)
  local o = {}
  o.src = src
  return setmetatable(o, CppObj)
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

--- An executable built from some collection of object files.
---@class object.Exe
local Exe = BuildObject:derive()
object.Exe = Exe

Exe.new = function(name, objs)
  local o = {}
  o.name = name
  o.objs = objs
  return setmetatable(o, Exe)
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

StaticLib.formPath = function(self_or_name, mount)
  local name =
    type(self_or_name) == "string" 
      and self_or_name
      or self_or_name.name

  local dir, base = name:match "(.*)/(.*)"
  if not dir then
    base = name
  end

  if sys.os == "linux" then
    return mount.."/"..dir.."/lib"..name..".a"
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
---@class object.Makefile
local Makefile = BuildObject:derive()
object.Makefile = Makefile

--- The directory make should run in.
---@param wdir string
--- A list specifying build objects we care about.
--- Because this build object should only be used for external projects, this
--- list indicates build objects from the project that we want dependents to
--- use.
---@param bobjs List | table
Makefile.new = function(wdir, bobjs)
  local o = {}
  o.wdir = wdir
  o.bobjs = {}
  for bobj in List(bobjs):each() do
    local list = bobjs[bobj.__type]
    if not list then
      list = List{}
      bobjs[bobj.__type] = list
    end
    list:push(bobj)
  end
  o.bobjs = bobjs
  return setmetatable(o, Makefile)
end

Makefile.getBuildObjects = function(self, BObj, list)
  local bobjs = self.bobjs[BObj]
  if bobjs then
    for bobj in bobjs:each() do
      -- TODO(sushi) make this more general, maybe have all build objects 
      --             only store a name like static libs do, and then provide
      --             a function taking a directory that they append the proper
      --             name to.
      if BObj == StaticLib then
        
      end
    end
  end
end

return object




