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
  ecs.report.ext.StaticLib "Ws2_32"
else
  error "unhandled os"
end

ecs.report.defines { ECS_VULKAN=1 }
if sys.os == "windows" then
  local vulkan_path = lake.getEnvVar("VULKAN_SDK");
  if not vulkan_path then
    error "VULKAN_SDK environment variable not defined"
  end
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
elseif sys.os == "linux" then
  ecs.report.dir.include
  {
    direct = { "../shaderc/include", }
  }

  ecs.report.dir.lib
  {
    direct = { "../shaderc/lib", }
  }

  ecs.report.ext.StaticLib "vulkan"
  ecs.report.ext.StaticLib "shaderc_combined"
end

if sys.cfg.mode == "debug" then
	ecs.report.defines { ECS_DEBUG=1 }
end

if sys.cfg.tracy.enabled then
  ecs.report.defines 
  { 
    TRACY_ENABLE=1,
    TRACY_CALLSTACK=8,
    TRACY_SAMPLING_HZ=35000,
  }

  if sys.cfg.tracy.wait_for_connection then
    ecs.report.defines { ECS_WAIT_FOR_TRACY_CONNECTION=1 }
  end

  -- Determine if the system has libunwind by pcalling luajit's 
  -- ffi.load (maybe there's a better way to do this :) and tell Tracy 
  -- about it if so (since its much faster to collect callstacks that way).
  -- I don't know if this is necessarily safe or a good way to do this, as 
  -- this will actually load the library.
  if sys.os == "linux" then
    local result = pcall(require "ffi" .load, "unwind")
    if result then
      ecs.report.defines { TRACY_LIBUNWIND_BACKTRACE=1 }
      ecs.report.ext.SharedLib "unwind"
    end
  end
end

for lfile in lake.find "src/**/*.lpp" :each() do
  ecs.report.LppObj(lfile)
end

for cfile in lake.find "src/**/*.cpp" :each() do
  if cfile:find "^src/external/tracy" then
    if cfile:find "TracyClient.cpp" then
      -- This file does the cool thing where it includes all the cpp
      -- files into another cpp file so we have to make sure we only 
      -- report this one.
      -- TODO(sushi) it would be nice to have a profiling lib for iro 
      --             which handles this instead but we only need it for 
      --             ecs for now.
      --             In fact since threading is an iro thing, we will need
      --             to in order to properly mark threads and mutexes and 
      --             such.
      ecs.report.CppObj(cfile)
    end
  else
    ecs.report.CppObj(cfile)
  end
end

local ecs_bobjs =
  ecs:gatherBuildObjects{bobj.LppObj, bobj.LuaObj, bobj.CppObj}

if sys.isProjectEnabled "hreload" then
  ecs:dependsOn "hreload"
  ecs.report.defines { ECS_HOT_RELOAD=1 }

  if sys.patch then
    ecs.report.SharedLib("ecs.patch"..sys.patch, ecs_bobjs,
    {
      luajit = true
    })

    local hrf = io.open(ecs:getBuildDir().."/ecs.hrf", "w")
    for bo in ecs_bobjs:each() do
      if bo:is(bobj.CppObj) or bo:is(bobj.LppObj) then
        hrf:write("+o", bo.proj:getBuildDir(), "/", bo:getTargetName(), "\n")
      end
    end
    hrf:close()
  else
    ecs.report.Exe("ecs", ecs_bobjs)
  end
else
  ecs.report.Exe("ecs", ecs_bobjs)
end
