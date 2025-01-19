local sys = require "build.sys"
local bobj = require "build.object"

local ecs = sys.getLoadingProject()

ecs:dependsOn "iro"

ecs:uses(sys.getOrLoadProject "lppclang".bobjs.published[bobj.SharedLib][1])

if sys.os == "linux" then
  ecs.report.ext.SharedLib "X11"
  ecs.report.ext.SharedLib "Xrandr"
  ecs.report.ext.SharedLib "Xcursor"
else
  error "unhandled os"
end

for lfile in lake.find "src/**/*.lpp" :each() do
  ecs.report.LppObj(lfile)
end

for cfile in lake.find "src/**/*.cpp" :each() do
  ecs.report.CppObj(cfile)
end

ecs.report.Exe("ecs", 
  ecs:gatherBuildObjects{bobj.LppObj, bobj.LuaObj, bobj.CppObj})
