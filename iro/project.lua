local enosi = require "enosi"
local recipes = require "recipes"
local proj = enosi.thisProject()
local driver = require "driver"

local List = require "list"

local os = lake.os()
local cwd = lake.cwd()

local mode = enosi.mode
local builddir = cwd.."/build/"..mode.."/"

proj:setCleaner(function()
  lake.rm(cwd.."/build", {recursive=true,force=true})
end)

local cpp_driver = driver.Cpp.new()

if enosi.getProject "notcurses" then
  proj:dependsOn "notcurses"
  cpp_driver.defines:push { "IRO_NOTCURSES", "1" }
end

proj:dependsOn "luajit"
proj:reportSharedLib "explain"
proj:reportIncludeDir(cwd)

cpp_driver.include_dirs:push { "src", proj:collectIncludeDirs() }

if mode == "debug" then
  cpp_driver.opt = "none"
  cpp_driver.debug_info = true
  proj:reportDefine { "IRO_DEBUG", "1" }
else
  cpp_driver.opt = "speed"
end

if os == "Linux" then
  proj:reportDefine { "IRO_LINUX", "1" }
elseif os == "Windows" then
  proj:reportDefine { "IRO_WIN32", "1" }
else
  error("unhandled OS for iro")
end

cpp_driver.defines = proj:collectDefines()

lake.find("iro/**/*.cpp"):each(function(cfile)
  local ofile = builddir..cfile..".o"
  local dfile = builddir..cfile..".d"
  cfile = cwd.."/"..cfile

  cpp_driver.input = cfile
  cpp_driver.output = ofile

  proj:reportObjFile(ofile)

  lake.target(ofile)
      :dependsOn { cfile, proj.path }
      :recipe(recipes.objFile(cpp_driver, proj))

  lake.target(dfile)
      :dependsOn(cfile)
      :recipe(
        recipes.depfile(
          driver.Depfile.fromCpp(cpp_driver),
          proj, ofile, dfile))
end)

local lua_driver = driver.LuaObj.new()

lake.find("iro/lua/*.lua"):each(function(lfile)
  local ofile = builddir..lfile..".o"
  lfile = cwd.."/"..lfile

  proj:reportObjFile(ofile)

  lua_driver.input = lfile
  lua_driver.output = ofile

  lake.target(ofile)
      :dependsOn { lfile, proj.path }
      :recipe(recipes.luaObjFile(lua_driver, proj))
end)
