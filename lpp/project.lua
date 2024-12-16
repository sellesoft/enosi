local sys = require "build.sys"
local helpers = require "build.helpers"
local bobj = require "build.object"

local lpp = sys.getLoadingProject()

lpp:dependsOn "iro"

for cfile in lake.find("src/**/*.cpp"):each() do
  lpp.report.CppObj(cfile)
end

for lfile in lake.find("src/**/*.lpp"):each() do
  lpp.report.LuaObj(lfile)
end

lpp.report.Exe("lpp", lpp.bobjs)




