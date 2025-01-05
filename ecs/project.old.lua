local enosi = require "enosi"
local recipes = require "recipes"
local Driver = require "driver"
local proj = enosi.thisProject()

local mode = enosi.mode
local cwd = lake.cwd()

local builddir = cwd.."/build/"..mode.."/"
lake.mkdir(builddir, {make_parents=true})

proj:setCleaner(function()
  lake.rm(cwd.."/build", {recursive = true, force = true})
end)

proj:dependsOn("iro")

local ecspath
if enosi.patch then
  ecspath = builddir.."ecs.patch"..enosi.patch..".so"
else
  ecspath = builddir.."ecs"
  proj:reportExecutable(ecspath)
end

local ecs = lake.target(ecspath)

local cpp_driver = Driver.Cpp.new()

if enosi.getProject "hreload" then
  proj:dependsOn "hreload"
  proj:reportDefine { "ECS_HRELOAD", "1" }
end

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
local lppdep_driver = Driver.LppDepFile.new()

lpp_driver.cpp = cpp_driver
lpp_driver.requires:push(cwd.."/src")
lppdep_driver.cpp = cpp_driver
lppdep_driver.requires:push(cwd.."/src")

local lppclang = enosi.getProject("lppclang")

if true then
lake.find("src/**/*.lpp"):each(function(lfile)
  local mfile = builddir..lfile..".meta"
  local cfile = builddir..lfile..".cpp"
  local ofile = cfile..".o"
  local dfile = cfile..".d"
  lfile = cwd.."/"..lfile

  lppdep_driver.input = lfile
  lppdep_driver.output = dfile

  cpp_driver.input = cfile
  cpp_driver.output = ofile
  lpp_driver.input = lfile
  lpp_driver.output = cfile
  lpp_driver.metafile = mfile
  lpp_driver.depfile = dfile

  lake.target(cfile)
    :dependsOn { lfile, lppclang.lib_path }
    :recipe(recipes.lpp(lpp_driver, proj))

  proj:reportObjFile(ofile)

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
if enosi.patch then
  link_driver.shared_lib = true
end

link_driver.shared_libs:push
{
  "X11",
  "Xrandr",
  "Xcursor"
}

ecs:recipe(recipes.linker(link_driver, proj))

local hrf = lake.target(builddir.."ecs.hrf")
hrf:dependsOn(ecs.path)
  :recipe(function()
    local f = io.open(hrf.path, "w")
    if not f then 
      error("failed to open '"..hrf.path.."' for writing")
    end

    -- Filter out luajit object files.
    -- TODO(sushi) get the absolute path. Need to setup projects storing their
    --             working directory.
    lake.find("../luajit/obj/*.o"):each(function(ofile)
      f:write("-o", ofile, "\n")
    end)

    --- Filter out external libs.
    f:write("-lexplain\n",
            "-lX11\n",
            "-lXrandr\n",
            "-lXcursor\n")

    -- We want all of our symbols.
    proj.artifacts.obj_files:each(function(ofile)
      f:write("+o", ofile, "\n")
    end)

    -- We want all of iro's symbols.
    enosi.getProject"iro".artifacts.obj_files:each(function(ofile)
      f:write("+o", ofile, "\n")
    end)

    -- Don't reload the reloaders functions.
    enosi.getProject"hreload".artifacts.obj_files:each(function(ofile)
      f:write("-o", ofile)
    end)
    f:close()
  end)
