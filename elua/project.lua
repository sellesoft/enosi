local sys = require "build.sys"
local List = require "list"
local bobj = require "build.object"

local elua = sys.getLoadingProject()

elua:dependsOn "iro"

local objs = List{}

for cfile in lake.find "src/**/*.cpp" :each() do
  objs:push(elua.report.CppObj(cfile))
end

elua.report.Exe("elua", 
  elua:gatherBuildObjects{bobj.CppObj, bobj.LuaObj})
