local enosi = require "enosi"
local recipes = require "recipes"
local Driver = require "driver"
local proj = enosi.thisProject()

local List = require "list"

local mode = enosi.mode
local cwd = lake.cwd()

local builddir = cwd.."/build/"..mode.."/"
lake.mkdir(builddir, {make_parents = true})

proj:setCleaner(function()
  lake.rm(builddir, {recursive = true, force = true})
end)

proj:dependsOn("llvm", "iro")

local lib = lake.target(builddir..enosi.getSharedLibName("lppclang"))
local exe = lake.target(builddir.."lppclang")

proj.lib_path = lib.path

local cpp_driver = Driver.Cpp.new()

cpp_driver.include_dirs:push { "src", proj:collectIncludeDirs() }

if mode == "debug" then
  cpp_driver.opt = "none"
  cpp_driver.debug_info = true
  proj:reportDefine { "LPPCLANG_DEBUG", "1" }
else
  cpp_driver.opt = "speed"
end

proj:reportDefine { "ENOSI_CLANG_EXECUTABLE", 
  '"'..enosi.getProject("llvm"):getExecutables()[1]..'"' }

-- need to export everything since we're compiling to a shared lib
cpp_driver.export_all = false

-- llvm does not compile with rtti enabled
cpp_driver.nortti = true
cpp_driver.defines = proj:collectDefines()

lake.find("src/**/*.cpp"):each(function(cfile)
  local ofile = builddir..cfile..".o"
  local dfile = builddir..cfile..".d"
  cfile = cwd.."/"..cfile

  proj:reportObjFile(ofile)

  cpp_driver.input = cfile
  cpp_driver.output = ofile

  lake.target(ofile):dependsOn { cfile, proj.path }
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

  proj:reportLuaObjFile(ofile)

  lua_driver.input = lfile
  lua_driver.output = ofile

  lake.target(ofile):dependsOn { lfile, proj.path }
      :recipe(recipes.luaObjFile(lua_driver, proj))
end)

exe:dependsOn(proj:collectObjFiles())
lib:dependsOn(proj:collectObjFiles())

local link_driver = Driver.Linker.new()

link_driver.static_libs = proj:collectStaticLibs()
link_driver.shared_libs = proj:collectSharedLibs()
link_driver.inputs = proj:collectObjFiles()
link_driver.libdirs = proj:collectLibDirs()

link_driver.output = exe.path
exe:recipe(recipes.linker(link_driver, proj))

link_driver.output = lib.path
link_driver.shared_lib = true
lib:recipe(recipes.linker(link_driver, proj))

