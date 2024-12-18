---
--- A library of code that results in some build objects.
---

local lake = require "lake"

local Type = require "Type"
local List = require "list"
local util = require "util"

local sys = require "build.sys"
local bobj = require "build.object"
local helpers = require "build.helpers"

local Project = Type.make()

local getBObj = function(k)
  return bobj[k] or
    error("no build object named "..k)
end

local tryQualifiers = function()
  
end

Project.new = function(name, wdir)
  local o = {}
  o.name = name
  o.wdir = wdir
  o.bobjs = 
  {
    private = {},
    public = {},
    external = 
    {
      private = {},
      public = {},
    }
  }
  o.dependencies = {}
  o.defines =
  {
    private = List{},
    public = List{},
  }
  o.dirs = {}

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

Project.constructReporter = function(proj)
  local defines = function(pub, defines)
    return function()
      proj:reportDefines(pub, defines)
    end
  end

  local obj = function(pub, ext, obj)
    if obj == "defines" then
      return function(defines)
        proj:reportDefines(pub, defines)
      end
    else
      return function(...)
        proj:reportBuildObject(pub, ext, getBObj(obj), ...)
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
    else
      return (obj(false, false, k))
    end
  end)

  return top 
end

Project.dependsOn = function(self, name)
  if self.dependencies[name] then
    sys.log:warn(
      "project ", self.name, " has already declared a dependency on", name, 
      "\n")
    return
  end

  self.dependencies[name] = sys.getOrLoadProject(name)
end

local getBObjList = function(bobjs, BObj)
  local list = bobjs[BObj]
  if not list then
    list = List{}
    bobjs[BObj] = list
  end
  return list
end

Project.reportBuildObject = function(self, pub, ext, BObj, ...)
  local bo = BObj.new(...)
  bo:assignMount(self.wdir.."/build/"..sys.cfg.mode)
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
end

--- Track an already existing build object.
Project.trackBuildObject = function(self, public, bobj)
  local map = public and self.bobjs.public or self.bobjs.private
  getBObjList(map, Type.getTypeOf(bobj)):push(bobj)
end

Project.reportDefines = function(self, pub, defines)
  local arr = pub and self.defines.public or self.defines.private
  for k,v in pairs(defines) do
    arr:push { k, tostring(v) }
  end
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

Project.getBuildObjects = function(self, BObj, list)
  getBuildObjects(self.bobjs.private, BObj, list)
end

Project.getPublicBuildObjects = function(self, BObj, list)
  getBuildObjects(self.bobjs.public, BObj, list)
  getBuildObjects(self.bobjs.external.public, BObj, list)
end

Project.getAllBuildObjects = function(self, BObj, list)
  self:getBuildObjects(BObj, list)
  self:getPublicBuildObjects(BObj, list)
end

Project.gatherPublicBuildObjects = function(self, BObj, list)
  for _,proj in pairs(self.dependencies) do
    proj:gatherPublicBuildObjects(BObj, list)
  end

  self:getPublicBuildObjects(BObj, list)
end

--- Gathers all public and private build objects of the given type 'BObj'
--- (retrieved from build/object.lua) from this project and all public build 
--- objects of the given type from projects this one depends on.
---
--- As a special case, any dependency that specifies a makefile build object 
--- also provides any objects specified in it.
Project.gatherBuildObjects = function(self, BObj, list)
  for _,proj in pairs(self.dependencies) do
    proj:gatherPublicBuildObjects(BObj, list)
  end

  self:getAllBuildObjects(BObj, list)
end

Project.getPublicDefines = function(self, list)
  for define in self.defines.public:each() do
    list:push(define)
  end
end 

Project.getDefines = function(self, list)
  for define in self.defines.private:each() do
    list:push(define)
  end
end

Project.gatherPublicDefines = function(self, list)
  for _,proj in pairs(self.dependencies) do
    proj:gatherPublicDefines(list)
  end
  self:getPublicDefines(list)
end

Project.gatherDefines = function(self, list)
  self:gatherPublicDefines(list)
  self:getDefines(list)
end

Project.formOutputPath = function(self, obj)
  return self.wdir.."/build/"..sys.cfg.mode.."/"..obj
end

Project.gatherDirs = function(self, dirname, list)
  for _,proj in pairs(self.dependencies) do
    proj:gatherDirs(dirname, list)
  end
  
  local dir = self.dirs[dirname]
  if dir then
    list:push(self.wdir.."/"..dirname)
  end
end 

return Project
