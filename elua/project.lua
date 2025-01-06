local sys = require "build.sys"
local bobj = require "build.object"

local elua = sys.getLoadingProject()

elua:dependsOn "iro"

for cfile in lake.find "src/**/*.cpp" :each() do
  elua.report.CppObj(cfile)
end

elua.report.Exe("elua", 
  elua:gatherBuildObjects{bobj.CppObj, bobj.LuaObj})
