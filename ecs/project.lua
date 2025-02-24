local sys = require "build.sys"
local bobj = require "build.object"

local ecs = sys.getLoadingProject()

ecs:dependsOn "iro"

ecs:uses(sys.getOrLoadProject "lppclang".bobjs.published[bobj.SharedLib][1])

if sys.os == "linux" then
  ecs.report.ext.SharedLib "X11"
  ecs.report.ext.SharedLib "Xrandr"
  ecs.report.ext.SharedLib "Xcursor"
elseif sys.os == "windows" then
  ecs.report.ext.SharedLib "user32"
  ecs.report.ext.SharedLib "gdi32"
else
  error "unhandled os"
end

if sys.cfg.renderer == "vulkan" then
  ecs.report.defines { ECS_VULKAN=1 }
  if sys.os == "windows" then
    local vulkan_path = lake.getEnvVar("VULKAN_SDK");
    vulkan_path = string.gsub(vulkan_path, "\\", "/");
    if vulkan_path:sub(-1) == "\0" then
      vulkan_path = vulkan_path:sub(1,#vulkan_path-1)
    end

    ecs.report.dir.include
    {
      direct =
      {
        "../shaderc/include",
        vulkan_path.."/Include",
      }
    }

    ecs.report.dir.lib
    {
      direct = {
        "../shaderc/lib",
        vulkan_path.."/Lib",
      }
    }

    ecs.report.ext.StaticLib "vulkan-1"
    ecs.report.ext.StaticLib "shaderc_combined"
  end
else
  ecs.report.defines { ECS_OPENGL=1 }
  if sys.os == "windows" then
    ecs.report.ext.SharedLib "opengl32"
  end
end

if sys.cfg.mode == "debug" then
	ecs.report.defines { ECS_DEBUG=1 }
end

for lfile in lake.find "src/**/*.lpp" :each() do
  ecs.report.LppObj(lfile)
end

for cfile in lake.find "src/**/*.cpp" :each() do
  ecs.report.CppObj(cfile)
end

ecs.report.Exe("ecs",
  ecs:gatherBuildObjects{bobj.LppObj, bobj.LuaObj, bobj.CppObj})
