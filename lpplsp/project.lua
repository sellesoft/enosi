local sys = require "build.sys"
local bobj = require "build.object"

local lpplsp = sys.getLoadingProject()

lpplsp:dependsOn "lpp"

for cfile in lake.find "src/**/*.cpp" :each() do
  lpplsp.report.CppObj(cfile)
end

for lfile in lake.find "src/**/*.lua" :each() do
  lpplsp.report.LuaObj(lfile)
end

local exe =
  lpplsp.report.Exe("lpplsp",
    lpplsp:gatherBuildObjects{bobj.CppObj, bobj.LuaObj})

-- lpplsp.report.published(lpplsp)
