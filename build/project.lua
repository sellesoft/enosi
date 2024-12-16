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

Project.new = function(name, wdir)
  local o = {}
  o.name = name
  o.wdir = wdir
  o.bobjs = 
  {
    private = {},
    public = {},
  }
  o.dependencies = {}
  o.defines =
  {
    private = List{},
    public = List{},
  }
  o.dirs = {}

  local reportDefines = function(arr, defines)
    for d,v in pairs(defines) do
      arr:push { d, v }
    end
  end

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

  local obj = function(pub, obj)
    if obj == "defines" then
      return function(defines)
        proj:reportDefines(pub, defines)
      end
    else
      return function(...)
        proj:reportBuildObject(pub, getBObj(obj), ...)
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
        return (obj(true, k))
      end)
    elseif k == "dir" then
      return dir  
    else
      return (obj(false, k))
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

local getBObjList = function(bobjs, public, BObj)
  local map = public and bobjs.public or bobjs.private
  local list = map[BObj]
  if not list then
    list = List{}
    map[BObj] = list
  end
  return list
end

Project.reportBuildObject = function(self, public, BObj, ...)
  local bo = BObj.new(...)
  getBObjList(self.bobjs, public, BObj)
    :push(bo)
end

Project.reportDefines = function(self, public, defines)
  local arr = public and self.defines.public or self.defines.private
  for k,v in pairs(defines) do
    arr:push { k, v }
  end
end

local function getBuildObjects(bobjs, BObj, list)
  util.dumpValue(BObj, 3)
  if bobj.BuildObject:isTypeOf(BObj) then
    list:pushList(getBObjList(bobjs, BObj))
  elseif List:isTypeOf(BObj) then
    for bo in BObj:each() do
      getBuildObjects(bobjs, bo, list)
    end
  else
    for _,bo in ipairs(bobjs) do
      getBuildObjects(bobjs, bo, list)
    end
  end
end

Project.getBuildObjects = function(self, BObj, list)
  getBuildObjects(self.bobjs.private, BObj, list)
end

Project.getPublicBuildObjects = function(self, BObj, list)
  getBuildObjects(self.bobjs.public, BObj, list)

  local makefiles = self.bobjs.private[bobj.Makefile]
  if makefiles then
    for makefile in makefiles:each() do
      local bobjs = makefile.bobjs[BObj]
      if bobjs then
      end
    end
  end
end

Project.getAllBuildObjects = function(self, BObj, list)
  self:getBuildObjects(BObj, list)
  self:getPublicBuildObjects(BObj, list)
end

Project.gatherPublicBuildObjects = function(self, BObj, list)
  

end

--- Gathers all public and private build objects of the given type 'BObj'
--- (retrieved from build/object.lua) from this project and all public build 
--- objects of the given type from projects this one depends on.
---
--- As a special case, any dependency that specifies a makefile build object 
--- also provides any objects specified in it.
Project.gatherBuildObjects = function(self, BObj)
  local o = List{}
  
  
end

return Project
