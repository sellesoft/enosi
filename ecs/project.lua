local enosi = require "enosi"
local recipes = require "recipes"
local Driver = require "driver"
local proj = enosi.thisProject()

local mode = enosi.mode
local cwd = lake.cwd()

local builddir = cwd.."/build/"..mode.."/"
lake.mkdir(builddir, {make_parents=true})

proj:setCleaner(function()
  lake.rm(builddir, {recursive = true, force = true})
end)

proj:dependsOn("iro")

local ecs = lake.target(builddir.."ecs")

proj:reportExecutable(ecs.path)

local cpp_driver = Driver.Cpp.new()

cpp_driver.include_dirs:push{ "src", proj:collectIncludeDirs() }

if mode == "debug" then
  cpp_driver.opt = "none"
  cpp_driver.debug_info = true
  proj:reportDefine { "ECS_DEBUG", "1" }
else
  cpp_driver.opt = "speed"
end

cpp_driver.defines = proj:collectDefines()

local lpp_driver = Driver.Lpp.new()

lpp_driver.cpp = cpp_driver
lpp_driver.requires:push(cwd.."/src")

if true then
lake.find("src/**/*.lpp"):each(function(lfile)
  local cfile = builddir..lfile..".cpp"
  local ofile = cfile..".o"
  lfile = cwd.."/"..lfile

  lpp_driver.input = lfile
  lpp_driver.output = cfile

  lake.target(cfile)
    :dependsOn(lfile)
    :recipe(recipes.lpp(lpp_driver, proj))

  proj:reportObjFile(ofile)

  cpp_driver.input = cfile
  cpp_driver.output = ofile

  lake.target(ofile)
    :dependsOn(cfile)
    :recipe(recipes.objFile(cpp_driver, proj))
end)
end

lake.find("src/**/*.cpp"):each(function(cfile)
  local ofile = builddir..cfile..".o"
  local dfile = builddir..cfile..".d"
  cfile = cwd.."/"..cfile

  proj:reportObjFile(ofile)

  cpp_driver.input = cfile
  cpp_driver.output = ofile

  lake.target(ofile):dependsOn(cfile)
      :recipe(recipes.objFile(cpp_driver, proj))

  lake.target(dfile):dependsOn(cfile)
      :recipe(recipes.depfile(
        Driver.Depfile.fromCpp(cpp_driver),
        proj, ofile, dfile))
end)

local lua_driver = Driver.LuaObj.new()

lake.find("src/*.lua"):each(function(lfile)
  local ofile = builddir..lfile..".o"
  lfile = cwd.."/"..lfile

  proj:reportObjFile(ofile)

  lua_driver.input = lfile
  lua_driver.output = ofile

  lake.target(ofile):dependsOn { lfile, proj.path }
      :recipe(recipes.luaObjFile(lua_driver, proj))
end)

ecs:dependsOn(proj:collectObjFiles())

local link_driver = Driver.Linker.new()

local inputs = proj:collectObjFiles()

link_driver.static_libs = proj:collectStaticLibs()
link_driver.shared_libs = proj:collectSharedLibs()
link_driver.inputs = inputs
link_driver.libdirs = proj:collectLibDirs()
link_driver.output = ecs.path

link_driver.shared_libs:push
{
  "X11",
  "Xrandr",
  "Xcursor"
}

ecs:recipe(recipes.linker(link_driver, proj))
