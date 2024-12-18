local sys = require "build.sys"
local helpers = require "build.helpers"
local bobj = require "build.object"
local List = require "list"

local lpp = sys.getLoadingProject()

lpp:dependsOn "iro"

for cfile in lake.find("src/**/*.cpp"):each() do
  lpp.report.CppObj(cfile)
end

for lfile in lake.find("src/*.lua"):each() do
  lpp.report.LuaObj(lfile)
end

local objs = List{}
lpp:gatherBuildObjects({bobj.CppObj, bobj.LuaObj}, objs)
lpp.report.Exe("lpp", objs)




