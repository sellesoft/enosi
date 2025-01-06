---
--- A library of code that results in some build objects.
---

local Type = require "Type"
local List = require "List"

local sys = require "build.sys"
local bobj = require "build.object"
local helpers = require "build.helpers"

---@class build.Project
---@field name string
--- The root directory of this project.
---@field root string
--- Other projects that this one depends on.
---@field dependencies table
--- Table of public and private build objects belonging to this project, as 
--- well as public and private external build objects this project depends on.
---@field bobjs table
--- Public and private defines for use when compiling C objects.
---@field defines table
--- Auxillary directories this project wants to make, eg. 'include' or 'lib'
--- dirs.
---@field dirs table
local Project = Type.make()

local getBObj = function(k)
  return bobj[k] or
    error("no build object named "..k)
end

Project.new = function(name, root)
  local o = {}
  o.name = name
  o.root = root
  o.bobjs = 
  {
    private = {},
    public = {},
    external = 
    {
      private = {},
      public = {},
    },
    published = {},
  }
  o.dependencies = {}
  o.defines =
  {
    private = List{},
    public = List{},
  }
  o.dirs = {}

  -- Tasks intended to group the various phases of building a project. 
  o.tasks = 
  {
    -- The 'root' task that all internal build objects created from this 
    -- project depend on. This task depends on the 'final' task of all of 
    -- this project's dependencies. 
    wait_for_deps = lake.task("wait for "..name.." deps"),

    -- Task that depends on all this project's internally build objects.
    build_internal = lake.task("build "..name.." internal objects"),

    -- Task that depends on the creation of any directory that this project
    -- specfies. This depends on 'build_internal'.
    resolve_dirs = lake.task("resolve "..name.." dirs"),

    -- The final task of this project, which its dependents will depend on.
    final = lake.task("finalize "..name),
  }

  o.tasks.build_internal:dependsOn(o.tasks.wait_for_deps)
  o.tasks.resolve_dirs:dependsOn(o.tasks.build_internal)
  o.tasks.final:dependsOn(o.tasks.resolve_dirs)

  o.report = Project.constructReporter(o)

  o.get = helpers.indexer(function(_,k)
    return (o:getBuildObjects(getBObj(k)))
  end)

  o.gather = helpers.indexer(function(_,k)
    return (o:gatherBuildObjects(getBObj(k)))
  end)

  return setmetatable(o, Project)
end

Project.__tostring = function(self)
  return "Project("..self.name..")"
end

local getBObjList = function(bobjs, BObj)
  local list = bobjs[BObj]
  if not list then
    list = List{}
    bobjs[BObj] = list
  end
  return list
end

local function getBuildObjects(bobjs, BObj, list)
  if bobj.BuildObject:isTypeOf(BObj) then
    if type(list) == "function" then
      for bobj in getBObjList(bobjs, BObj):each() do
        list(bobj)
      end
    else
      list:pushList(getBObjList(bobjs, BObj))
    end
  elseif List:isTypeOf(BObj) then
    for bo in BObj:each() do
      getBuildObjects(bobjs, bo, list)
    end
  else
    for _,bo in ipairs(BObj) do
      getBuildObjects(bobjs, bo, list)
    end
  end
end

Project.constructReporter = function(proj)
  local obj = function(pub, ext, obj)
    if obj == "defines" then
      return function(defines)
        proj:reportDefines(pub, defines)
      end
    else
      return function(...)
        return (proj:reportBuildObject(pub, ext, getBObj(obj), ...))
      end
    end
  end

  local dir = helpers.indexer(function(_,k)
    if proj.dirs[k] then
      sys.log:error("dir '",k,"' already defined for ",proj.name,"\n")
      error()
    end

    return function(def)
      proj.dirs[k] = def
    end
  end)

  local published = function(...)
    for bobj in List{...}:each() do
      local list = getBObjList(proj.bobjs.published, Type.of(bobj))
      list:push(bobj)
    end
  end

  local top = helpers.indexer(function(_,k)
    if k == "pub" then
      return helpers.indexer(function(_,k)
        return (obj(true, false, k))
      end)
    elseif k == "ext" then
      return helpers.indexer(function(_,k)
        if k == "pub" then
          return helpers.indexer(function(_,k)
            return (obj(true, true, k))
          end)
        else
          return (obj(false, true, k))
        end
      end)
    elseif k == "dir" then
      return dir  
    elseif k == "published" then
      return published
    else
      return (obj(false, false, k))
    end
  end)

  return top 
end

Project.dependsOn = function(self, ...)
  for name in List{...}:each() do
    if self.dependencies[name] then
      sys.log:warn(
        "project ", self.name, " has already declared a dependency on", name, 
        "\n")
      return
    end

    local dep = sys.getOrLoadProject(name)
    self.dependencies[name] = dep
    self.tasks.wait_for_deps:dependsOn(dep.tasks.final)
  end
end

--- Declares that this project uses some build objects that are from other 
--- projects without assuming that it uses those objects to build stuff.
---
--- This is a bit scuffed in that this is not tracked anywhere like normal 
--- dependencies are, but its probably ok for now. This is being added for a 
--- case like ecs using lppclang's shared library during compilation of lpp 
--- files. If we were to say ecs:dependsOn("lppclang") then the shared library 
--- would be linked into ecs, where it would never be used.
--- 
--- All this does is make this project's 'wait_for_deps' task dependent on 
--- whatever build objects are provided here, as long as they have a task.
---
--- This is a very weak part of the build system that I really don't like. It
--- makes me wonder if it would be better to instead have it so that a 
--- project's dependencies on other projects can specify what parts of that 
--- project it actually needs, or how it uses that project. Very odd cases.
Project.uses = function(self, ...)
  for bobj in List{...}:each() do
    print(self.name, "uses", bobj.task.name)
    self.tasks.wait_for_deps:dependsOn(bobj.task)
  end
end

Project.reportBuildObject = function(self, pub, ext, BObj, ...)
  local bo = BObj.new(...)
  bo.proj = self
  if bo.declareTask then
    bo:declareTask()
    if not ext then
      self.tasks.build_internal:dependsOn(bo.task)
      bo.task:dependsOn(self.tasks.wait_for_deps)
    end
  end
  local map
  if pub then
    if ext then
      map = self.bobjs.external.public
    else
      map = self.bobjs.public
    end
  else
    if ext then
      map = self.bobjs.external.private
    else
      map = self.bobjs.private
    end
  end
  getBObjList(map, BObj):push(bo)
  return bo
end

--- Track an already existing build object.
Project.trackBuildObject = function(self, public, bobj)
  local map = public and self.bobjs.public or self.bobjs.private
  getBObjList(map, Type.of(bobj)):push(bobj)
end

Project.reportDefines = function(self, pub, defines)
  local arr = pub and self.defines.public or self.defines.private
  for k,v in pairs(defines) do
    arr:push { k, tostring(v) }
  end
end

Project.getBuildObjects = function(self, BObj, list)
  list = list or List{}
  getBuildObjects(self.bobjs.private, BObj, list)
  getBuildObjects(self.bobjs.external.private, BObj, list)
  return list
end

Project.getPublicBuildObjects = function(self, BObj, list)
  list = list or List{}
  getBuildObjects(self.bobjs.public, BObj, list)
  getBuildObjects(self.bobjs.external.public, BObj, list)
  return list
end

Project.getAllBuildObjects = function(self, BObj, list)
  list = list or List{}
  self:getBuildObjects(BObj, list)
  self:getPublicBuildObjects(BObj, list)
  return list
end

Project.gatherPublicBuildObjects = function(self, BObj, list)
  list = list or List{}
  for _,proj in pairs(self.dependencies) do
    proj:gatherPublicBuildObjects(BObj, list)
  end

  self:getPublicBuildObjects(BObj, list)
  return list
end

--- Gathers all public and private build objects of the given type 'BObj'
--- (retrieved from build/object.lua) from this project and all public build 
--- objects of the given type from projects this one depends on.
---
--- As a special case, any dependency that specifies a makefile build object 
--- also provides any objects specified in it.
Project.gatherBuildObjects = function(self, BObj, list)
  list = list or List{}
  for _,proj in pairs(self.dependencies) do
    proj:gatherPublicBuildObjects(BObj, list)
  end

  self:getAllBuildObjects(BObj, list)
  return list
end

Project.getPublicDefines = function(self, list)
  list = list or List{}
  for define in self.defines.public:each() do
    list:push(define)
  end
  return list
end 

Project.getDefines = function(self, list)
  list = list or List{}
  for define in self.defines.private:each() do
    list:push(define)
  end
  return list
end

Project.gatherPublicDefines = function(self, list)
  list = list or List{}
  for _,proj in pairs(self.dependencies) do
    proj:gatherPublicDefines(list)
  end
  self:getPublicDefines(list)
  return list
end

Project.gatherDefines = function(self, list)
  list = list or List{}
  self:gatherPublicDefines(list)
  self:getDefines(list)
  return list
end

Project.formOutputPath = function(self, obj)
  return self.root.."/build/"..sys.cfg.mode.."/"..obj
end

--- Attempts to get an absolute path to an auxillary dir for this project.
--- If the project did not specify the given dir, then nil is returned.
Project.getAuxDirPath = function(self, name)
  if self.dirs[name] then
    return self:getBuildRootDir().."/"..name
  end
end

Project.gatherDirs = function(self, dirname, list)
  list = list or List{}
  for _,proj in pairs(self.dependencies) do
    proj:gatherDirs(dirname, list)
  end

  local dir = self.dirs[dirname]
  if dir then
    if dir.direct then
      for direct in List(dir.direct):each() do
        list:push(direct)
      end
    else
      list:push(self:getAuxDirPath(dirname))
    end
  end
  return list
end 

--- Forms a path to this project's build root.
---@return string
Project.getBuildRootDir = function(self)
  return self.root.."/build"
end

--- Forms a path to the build directory used for this project in the currently
--- selected build mode.
Project.getBuildDir = function(self)
  return self:getBuildRootDir().."/"..sys.cfg.mode
end

--- Cleans all build artifacts generated by this project.
---
--- For all projects this will recursively delete the generated build dir 
--- if one exists. Some projects, probably only external ones, may specify 
--- a 'cleaner' that implements some special behavior for cleaning up build
--- artifacts, eg. external projects that use make may run 'make clean'.
Project.clean = function(self)
  local bdir = self:getBuildRootDir()
  if lake.pathExists(bdir) then
    lake.rm(bdir, {recursive = true, force = true})
  end
  if self.cleaner then
    self:cleaner()
  end
end

Project.publish = function(self)

end

return Project
