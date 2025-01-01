local sys = require "build.sys"
local helpers = require "build.helpers"
local bobj = require "build.object"
local List = require "list"

local lpp = sys.getLoadingProject()

lpp:dependsOn "iro"

local objs = List{}

for cfile in lake.find("src/**/*.cpp"):each() do
  objs:push(lpp.report.CppObj(cfile))
end

for lfile in lake.find("src/*.lua"):each() do
  objs:push(lpp.report.LuaObj(lfile))
end

lpp.report.Exe("lpp", 
  lpp:gatherBuildObjects{bobj.CppObj, bobj.LuaObj})

