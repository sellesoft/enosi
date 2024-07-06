local enosi = require "enosi"
local proj = enosi.thisProject()
local recipes = require "recipes"
local Driver = require "driver"

local list = require "list"

local mode = enosi.mode
local cwd = lake.cwd()

local builddir = cwd.."/build/"..mode.."/"
lake.mkdir(builddir, {make_parents = true})

proj:setCleaner(function()
  lake.rm(builddir, {recursive = true, force = true})
end)

proj:dependsOn "iro"

local exe = lake.target(builddir.."lake")
proj:reportExecutable(exe.path)

local cpp_driver = Driver.Cpp.new()

cpp_driver.include_dirs:push { "src", proj:collectIncludeDirs() }

if mode == "debug" then
  cpp_driver.opt = "none"
  cpp_driver.debug_info = true
  proj:reportDefine { "LAKE_DEBUG", "1" }
else
  cpp_driver.opt = "speed"
end

cpp_driver.defines = proj:collectDefines()

lake.find("src/**/*.cpp"):each(function(cfile)
  local ofile = builddir..cfile..".o"
  local dfile = builddir..cfile..".d"
  cfile = cwd.."/"..cfile

  cpp_driver.input = cfile
  cpp_driver.output = ofile

  proj:reportObjFile(ofile)

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

exe:dependsOn(proj:collectObjFiles())

local link_driver = Driver.Linker.new()

link_driver.static_libs = proj:collectStaticLibs()
link_driver.shared_libs = proj:collectSharedLibs()
link_driver.inputs = proj:collectObjFiles()
link_driver.libdirs = proj:collectLibDirs()
link_driver.output = exe.path

exe:recipe(recipes.linker(link_driver, proj))


