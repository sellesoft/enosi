--- 
--- Internal representations of 'build objects', which are anything that is
--- built and/or used to build other build objects.
---

local sys = require "build.sys"
local Type = require "Type"
local List = require "List"
local helpers = require "build.helpers"
local buffer = require "string.buffer"
local flair = require "build.flair"

local build = 
{
  sys = require "build.sys",
  cmd = require "build.commands",
}

--- * =========================================================================
---
---   Helpers
---
--- * =========================================================================

local tryLoadDepFile = function(dfile, obj_task)
  local depfile = io.open(dfile, "r")
  if depfile then
    for file in depfile:lines() do
      obj_task:dependsOn(lake.task(file))
    end
    depfile:close()
  end
end

-- * --------------------------------------------------------------------------

local setFileExistanceAndModTimeCondition = function(task)
  task:cond(function(prereqs)
    if not lake.pathExists(task.name) then
      return true
    end

    local target_modtime = lake.modtime(task.name)
    for prereq in prereqs:each() do
      if lake.modtime(prereq) > target_modtime then
        return true
      end
    end
  end)
end

-- * --------------------------------------------------------------------------

local ensureDirExists = function(path)
  local dir = helpers.getPathDirname(path)
  if dir then
    lake.mkdir(dir, {make_parents = true})
  end
end

-- * --------------------------------------------------------------------------

--- Wrapper around lake's cmd that gets extra information we care about.
local run = function(args)
  local start = lake.getMonotonicClock()
  local capture = buffer.new()
  local result = lake.cmd(args, { onRead = function(s) capture:put(s) end })
  return result, capture:get(), (lake.getMonotonicClock() - start) / 1000000
end

-- * --------------------------------------------------------------------------

--- Another wrapper around 'run' which handles reporting the result as well
--- as calling 'error' on failure.
local runAndReportResult = function(args, infile, outfile)
  local result, output, time = run(args)

  if result ~= 0 then
    flair.writeFailure(outfile)
  else
    if infile then
      flair.writeSuccessInputToOutput(infile, outfile, time)
    else
      flair.writeSuccessOnlyOutput(outfile, time)
    end
  end

  io.write(output)

  if result ~= 0 then
    lake.throwRecipeErr()
  end
end

--- * =========================================================================
---
---   Module
---
--- * =========================================================================

local object = {}

--- * =========================================================================
---
---   Build Objects
---
--- * =========================================================================

--- The base of all build objects.
---@class object.BuildObject : iro.Type
--- The project this build object comes from.
---@field proj build.Project
--- The lake Task that will build this object, if any.
---@field task Task?
local BuildObject = Type.make()
object.BuildObject = BuildObject

BuildObject.__tostring = function(self)
  return "BuildObject("..self.bobj_name..")"
end

BuildObject.getOutputPath = function(self)
  return self.proj:getBuildDir().."/"..self:getTargetName()
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
  end
})

-- * --------------------------------------------------------------------------

--- An object file of some kind. This class is not meant to be used directly,
--- instead a derived type, such as CppObj, should be used to indicate where
--- the obj file comes from (so we know how to build it). This class should
--- be used to generically determine if a build object is an obj file, eg.
---
---  if bobj:is(object.Obj) then
---    ...
---  end
---
---@class object.Obj : object.BuildObject
--- The file this obj will be built from.
---@field src string
local Obj = BuildObject:derive()

-- * --------------------------------------------------------------------------

--- Gets the full target name of this obj file.
Obj.getTargetName = function(self)
  return self.src..".o"
end

-- * --------------------------------------------------------------------------

--- An obj file built from a cpp file.
---@class object.CppObj : object.Obj
--- The cpp file we are building it from.
---@field src string
local CppObj = Obj:derive()
object.CppObj = CppObj

-- * --------------------------------------------------------------------------

---@param src string 
---@return object.CppObj
CppObj.new = function(src)
  local o = {}
  o.src = src
  return setmetatable(o, CppObj)
end

-- * --------------------------------------------------------------------------

CppObj.declareTask = function(self)
  self.task = 
    lake.task(self:getOutputPath())
      :workingDirectory(self.proj.root)
end

-- * --------------------------------------------------------------------------

---@param cmd cmd.CppObj
CppObj.defineTask = function(self, cmd)
  local cfile = self.proj.root.."/"..self.src
  local ofile = self.proj:getBuildDir().."/"..self.src..".o"
  local dfile = self.proj:getBuildDir().."/"..self.src..".d"
  local cpp_cmd = cmd:complete(cfile, ofile)

  ensureDirExists(ofile)

  ---@type cmd.CppDeps.Params
  local dep_params = {}

  dep_params.compiler = cmd.params.compiler
  dep_params.include_dirs = cmd.params.include_dirs
  dep_params.defines = cmd.params.defines

  local dep_cmd = build.cmd.CppDeps.new(dep_params):complete(self.src)

  local cpp_task = lake.task(cfile)
  local obj_task = self.task or error("nil CppObj.task")
  local dep_task = lake.task(dfile)

  obj_task
    :dependsOn(cpp_task)
    :workingDirectory(self.proj.root)
  dep_task
    :dependsOn(cpp_task)
    :workingDirectory(self.proj.root)

  tryLoadDepFile(dfile, obj_task)

  setFileExistanceAndModTimeCondition(obj_task)
  setFileExistanceAndModTimeCondition(dep_task)

  obj_task
    :recipe(function()
      runAndReportResult(cpp_cmd, cfile, ofile)
    end)

  dep_task
    :recipe(function()
      local result, output = run(dep_cmd)
      
      local depfile = io.open(dfile, "w")
      if not depfile then
        error("failed to open depfile at "..dfile)
      end
    
      for f in output:gmatch("%S+") do
        if f:sub(-1) ~= ":" and 
           f ~= "\\" 
        then
          local fullpath = f
          if f:sub(1,1) ~= "/" then
            fullpath = self.proj.root.."/"..f
          end
          fullpath = fullpath:gsub("/[^/]-/%.%.", "")
          depfile:write(fullpath, "\n")
        end
      end

      depfile:close()
    end)
end

-- * --------------------------------------------------------------------------

--- An obj file built from a lua file.
---@class object.LuaObj : object.Obj
local LuaObj = Obj:derive()
object.LuaObj = LuaObj

-- * --------------------------------------------------------------------------

---@param src string
---@return object.LuaObj
LuaObj.new = function(src)
  local o = {}
  o.src = src
  return setmetatable(o, LuaObj)
end

-- * --------------------------------------------------------------------------

LuaObj.declareTask = function(self)
  self.task = 
    lake.task(self:getOutputPath())
      :workingDirectory(self.proj.root)
end

-- * --------------------------------------------------------------------------

LuaObj.defineTask = function(self, cmd)
  local lfile = self.proj.root.."/"..self.src
  local ofile = self.task.name
  ensureDirExists(ofile)

  local comp = cmd:complete(self.src, ofile)

  setFileExistanceAndModTimeCondition(self.task)

  self.task
    :recipe(function()
      runAndReportResult(comp, lfile, ofile)
    end)
end

-- * --------------------------------------------------------------------------

--- An obj file built from an lpp file.
---@class object.LppObj : object.Obj
local LppObj = Obj:derive()
object.LppObj = LppObj

-- * --------------------------------------------------------------------------

---@param src string
---@return object.LppObj
LppObj.new = function(src)
  local o = {}
  o.src = src
  return setmetatable(o, LppObj)
end

-- * --------------------------------------------------------------------------

LppObj.declareTask = function(self)
  self.task = 
    lake.task(self:getOutputPath())
      :workingDirectory(self.proj.root)

  lake.task(self.proj:getBuildDir().."/"..self.src..".cpp")
    :workingDirectory(self.proj.root)
    :dependsOn(self.proj.tasks.wait_for_deps)
end

-- * --------------------------------------------------------------------------

LppObj.getTargetName = function(self)
  return self.src..".cpp.o"
end   

-- * --------------------------------------------------------------------------

---@param cmd cmd.LppObj
LppObj.defineTask = function(self, cmd)
  local out = self.task.name
  lake.mkdir(helpers.getPathDirname(out), { make_parents = true })
  
  local lfile = self.proj.root.."/"..self.src
  local cfile = self.proj:getBuildDir().."/"..self.src..".cpp"
  local ofile = cfile..".o"
  local mfile = self.proj:getBuildDir().."/"..self.src..".meta"
  local dfile = self.proj:getBuildDir().."/"..self.src..".d"

  ensureDirExists(ofile)

  local lpp_cmd, cpp_cmd = cmd:complete(lfile, cfile, ofile, dfile, mfile)

  local lpp_task = lake.task(lfile)
  local cpp_task = lake.task(cfile)
  local obj_task = lake.task(ofile)

  cpp_task
    :dependsOn(lpp_task)
    :workingDirectory(self.proj.root)
  obj_task
    :dependsOn(cpp_task)
    :workingDirectory(self.proj.root)

  tryLoadDepFile(dfile, obj_task)
  
  setFileExistanceAndModTimeCondition(cpp_task)
  setFileExistanceAndModTimeCondition(obj_task)
  
  cpp_task:recipe(function()
    runAndReportResult(lpp_cmd, lfile, cfile)
  end)

  obj_task:recipe(function()
    runAndReportResult(cpp_cmd, cfile, ofile)
  end)
end

-- * --------------------------------------------------------------------------

--- An executable built from some collection of object files.
---@class object.Exe : object.BuildObject
--- The name of the executable.
---@field name string
--- The object files used to build the executable.
---@field objs List
local Exe = BuildObject:derive()
object.Exe = Exe

-- * --------------------------------------------------------------------------

Exe.new = function(name, objs)
  local o = {}
  o.name = name
  o.objs = objs
  return setmetatable(o, Exe)
end

-- * --------------------------------------------------------------------------

Exe.getTargetName = function(self)
  return self.name
end

-- * --------------------------------------------------------------------------

Exe.declareTask = function(self)
  self.task = 
    lake.task(self:getOutputPath())
      :workingDirectory(self.proj.root)
end

-- * --------------------------------------------------------------------------

local defineLinkerTask = function(self, is_shared)
  local out = self:getOutputPath()

  ---@type cmd.Exe.Params | {}
  local params = {}

  params.linker = sys.cfg.cpp.linker

  params.libdirs = List{}
  self.proj:gatherDirs("lib",params.libdirs)

  params.static_libs = List{}
  self.proj:gatherBuildObjects(object.StaticLib, function(bobj)
    self.task:dependsOn(bobj.task)
    params.static_libs:push(bobj.name)
  end)

  params.shared_libs = List{}
  self.proj:gatherBuildObjects(object.SharedLib, function(bobj)
    if bobj ~= self then
      self.task:dependsOn(bobj.task)
      params.shared_libs:push(bobj.name)
    end
  end)

  params.is_shared = is_shared

  local cmd = build.cmd.Exe.new(params)

  local objs = List{}
  for obj in self.objs:each() do
    objs:push(obj:getOutputPath())
    if obj.task then
      self.task:dependsOn(obj.task)
    else
      sys.log:warn("exe obj '",tostring(obj),"' does not have a task set!\n")
    end
  end

  local exe_cmd = cmd:complete(objs, out)

  setFileExistanceAndModTimeCondition(self.task)

  self.task
    :recipe(function()
      runAndReportResult(exe_cmd, nil, self.task.name)
    end)
end

-- * --------------------------------------------------------------------------

Exe.defineTask = function(self)
  defineLinkerTask(self, false)
end

-- * --------------------------------------------------------------------------

--- A static library.
--- @class object.StaticLib : object.BuildObject
local StaticLib = BuildObject:derive()
object.StaticLib = StaticLib

-- * --------------------------------------------------------------------------

StaticLib.new = function(name, objs)
  local o = {}
  o.name = name
  o.objs = objs
  return setmetatable(o, StaticLib)
end

-- * --------------------------------------------------------------------------

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

-- * --------------------------------------------------------------------------

local SharedLib = BuildObject:derive()
object.SharedLib = SharedLib

-- * --------------------------------------------------------------------------

SharedLib.new = function(name, objs)
  local o = {}
  o.name = name
  o.objs = objs
  return setmetatable(o, SharedLib)
end

-- * --------------------------------------------------------------------------

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

-- * --------------------------------------------------------------------------

SharedLib.declareTask = function(self)
  self.task = 
    lake.task(self:getOutputPath())
      :workingDirectory(self.proj.root)
end

-- * --------------------------------------------------------------------------

SharedLib.defineTask = function(self)
  defineLinkerTask(self, true)
end

-- * --------------------------------------------------------------------------

--- A special build object that specifies a makefile that needs to be run.
--- This should only be used for compiling external libraries that use 
--- make for compilation. 
---@class object.Makefile : object.BuildObject
local Makefile = BuildObject:derive()
object.Makefile = Makefile

-- * --------------------------------------------------------------------------

--- The directory make should run in.
---@param root string
Makefile.new = function(root, cond)
  local o = {}
  o.root = root
  o.cond = cond or function()
    return 0 ~= os.execute "make -q"
  end
  return setmetatable(o, Makefile)
end

-- * --------------------------------------------------------------------------

Makefile.getOutputPath = function()
  error("getOutputPath should never be called on a Makefile")
end

-- * --------------------------------------------------------------------------

Makefile.declareTask = function(self)
  self.task = 
    lake.task("make "..self.proj.name)
      :cond(self.cond)
      :workingDirectory(self.proj.root.."/"..self.root)
end

-- * --------------------------------------------------------------------------

Makefile.defineTask = function(self)
  self.task
    :recipe(function()
      local result = lake.cmd({"make", "-j"}, 
      {
        onRead = io.write
      })
    end)
end

-- * --------------------------------------------------------------------------

--- A special build object that specifies a CMake project thing that needs
--- to be configured and run.
---@class object.CMake : object.BuildObject
--- Path to the configuration file.
---@field config string
--- Path where build artifacts should be placed.
---@field output string
--- The build mode to be used.
---@field mode string
local CMake = BuildObject:derive()
object.CMake = CMake

-- * --------------------------------------------------------------------------

--- Path to the configuration file.
---@param config string
--- Path where build artifacts should be placed.
---@param output string
--- The build mode to be used.
---@param mode string
CMake.new = function(config, output, mode)
  local o = {}
  o.config = config
  o.output = output
  o.mode = mode
  return setmetatable(o, CMake)
end

-- * --------------------------------------------------------------------------

CMake.getOutputPath = function()
  error("getOutputPath should never be called on a CMake")
end

-- * --------------------------------------------------------------------------

CMake.declareTask = function(self)
  self.task = 
    lake.task("cmake "..self.output)
      :workingDirectory(self.output)
      :cond(function()
        return not lake.pathExists("CMakeCache.txt")
      end)
end

-- * --------------------------------------------------------------------------

CMake.defineTask = function(self)
  self.task:recipe(function()
    local args = List
    {
			"-DLLVM_CCACHE_BUILD=ON",
			"-DLLVM_OPTIMIZED_TABLEGEN=ON",
			"-DLLVM_ENABLE_PROJECTS=clang;lld",
			"-DCMAKE_BUILD_TYPE="..self.mode,
			"-DLLVM_USE_LINKER=lld", -- TODO(sushi) support for other linkers
    }

    local result = lake.cmd(
      { "cmake", "-G", "Unix Makefiles", self.config, args },
      { onRead = io.write })

    if result ~= 0 then
      sys.log:error("failed to configure cmake")
    end

    local result = lake.cmd(
      { "make", "-j6" },
      { onRead = io.write })

    if result ~= 0 then
      sys.log:error("failed to run make for cmake project")
    end
  end)
end

return object




