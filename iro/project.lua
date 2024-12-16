local sys = require "build.sys"

local iro = sys.getLoadingProject()

iro:dependsOn "luajit"

iro.report.dir.include
{
  from = "iro",
  glob = "**/*.h"
}

if sys.cfg.mode == "debug" then
  iro.report.pub.defines { IRO_DEBUG=1 }
end

if sys.os == "linux" then
  iro.report.pub.defines { IRO_LINUX=1 }
  iro.report.pub.ExternalSharedLib "explain"
elseif sys.os == "windows" then
  iro.report.pub.defines { IRO_WIN32=1 }
else
  error("unhandled OS")
end

if sys.cfg.cpp.compiler == "clang++" then
  iro.report.pub.defines { IRO_CLANG=1 }
elseif sys.cfg.cpp.compiler == "cl" then
  iro.report.pub.defines { IRO_CL=1 }
end

for cfile in lake.find("src/**/*.cpp"):each() do
  iro.report.pub.CppObj(cfile)
end

for lfile in lake.find("iro/lua/*.lua"):each() do
  iro.report.pub.LuaObj(lfile)
end
