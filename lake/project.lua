local sys = require "build.sys"
local bobj = require "build.object"

local proj = sys.getLoadingProject()

proj:dependsOn "iro"

for cfile in lake.find "src/**/*.cpp" :each() do
  proj.report.CppObj(cfile)
end

for lfile in lake.find "src/*.lua" :each() do
  proj.report.LuaObj(lfile)
end

local exe = 
  proj.report.Exe("lake",
    proj:gatherBuildObjects{bobj.CppObj, bobj.LuaObj})

proj.report.published(exe)
