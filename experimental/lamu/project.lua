local sys = require "build.sys"
local bobj = require "build.object"

local lamu = sys.getLoadingProject()

lamu:dependsOn "iro"

for cfile in lake.find "**/*.cpp" :each() do
  lamu.report.CppObj(cfile)
end

for lfile in lake.find "**/*.lua" :each() do
  lamu.report.LuaObj(lfile)
end

lamu.report.Exe("lamu", 
  lamu:gatherBuildObjects{bobj.CppObj, bobj.LuaObj})
