local sys = require "build.sys"
local bobj = require "build.object"

local lpp = sys.getLoadingProject()

lpp:dependsOn "iro"

lpp.report.dir.include
{
  from = "src",
  to = "lpp",
  glob = "*.h"
} 

for cfile in lake.find("src/**/*.cpp"):each() do
  lpp.report.CppObj(cfile)
end

for lfile in lake.find("src/**/*.lua"):each() do
  lpp.report.LuaObj(lfile)
end

-- lpp.report.ext.SharedLib "z"

local exe = 
  lpp.report.Exe("lpp", 
    lpp:gatherBuildObjects{bobj.CppObj, bobj.LuaObj})

lpp.report.published(exe)


