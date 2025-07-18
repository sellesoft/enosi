local sys = require "build.sys"

local iro = sys.getLoadingProject()

iro:dependsOn "luajit"

iro.report.dir.include
{
  from = "src",
  to = "iro",
  glob = "**/*.h"
}

if sys.cfg.mode == "debug" then
  iro.report.pub.defines { IRO_DEBUG=1 }
end

if sys.os == "linux" then
  iro.report.pub.defines { IRO_LINUX=1 }
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

if sys.cfg.iro and sys.cfg.iro.break_on_error_log then
  iro.report.pub.defines { IRO_LOGGER_DEBUG_BREAK_ON_ERRORS=1 }
end

for cfile in lake.find("src/**/*.cpp"):each() do
  iro.report.pub.CppObj(cfile)
end

for lfile in lake.find("src/lua/*.lua"):each() do
  iro.report.pub.LuaObj(lfile)
end
