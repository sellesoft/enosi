local sys = require "build.sys"
local bobj = require "build.object"

local fib = sys.getLoadingProject()

fib:dependsOn "iro"

for cfile in lake.find "**/*.cpp" :each() do
  fib.report.CppObj(cfile)
end

for afile in lake.find "**/*.a" :each() do
  fib.report.AsmObj(afile)
end

fib.report.Exe("fib", fib:gatherBuildObjects{bobj.CppObj, bobj.AsmObj})
