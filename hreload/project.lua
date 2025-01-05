local sys = require "build.sys"

local hreload = sys.getLoadingProject()

hreload:dependsOn "iro"

hreload.report.dir.include 
{
  from = "src",
  to = "hreload",
  glob = "**/.h",
}

for cfile in lake.find("src/**/*.cpp"):each() do
  hreload.report.pub.CppObj(cfile)
end
