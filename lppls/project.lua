local sys = require "build.sys"
local bobj = require "build.object"

local lppls = sys.getLoadingProject()

lppls:dependsOn "lpp"

for cfile in lake.find "src/**/*.cpp" :each() do
  lppls.report.CppObj(cfile)
end

for lfile in lake.find "src/**/*.lua" :each() do
  lppls.report.LuaObj(lfile)
end

local exe =
  lppls.report.Exe("lppls",
    lppls:gatherBuildObjects{bobj.CppObj, bobj.LuaObj})

-- lppls.report.published(lppls)
