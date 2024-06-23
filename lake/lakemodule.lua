local options = assert(lake.getOptions())

local List = require "list"

local mode = options.mode or "debug"
local build_dir = lake.cwd().."/build/"..mode.."/"

options.registerCleaner(function()
  lake.rm(build_dir, {recursive = true, force = options.force_clean})
end)

lake.mkdir(build_dir, {make_parents = true})

local lakeexe = lake.target(build_dir.."lake")

local report = assert(options.report)
local reports = assert(options.reports)

assert(reports.iro.objFiles, "lake depends on iro's object files but they were not found in the reports table. Was iro's module imported? Maybe it was imported after this one.")

local recipes = assert(options.recipes)

report.executable(tostring(lakeexe))

local cflags = List
{
  "-Isrc",
  "-DIRO_LINUX=1",
  options.getProjIncludeDirFlags "iro"
}

if mode == "debug" then
  cflags:push { "-O0", "-ggdb0", "-DLAKE_DEBUG=1" }
else
  cflags:push "-O2"
end

local lflags = List
{
  options.getProjLibs "luajit"
}

local c_files = lake.find("src/**/*.cpp")

for c_file in c_files:each() do
  local o_file = c_file:gsub("(.-)%.cpp", build_dir.."%1.o")
  local d_file = o_file:gsub("(.-)%.o", "%1.d")

  report.objFile(o_file)

  lake.target(o_file)
      :dependsOn(c_file)
      :recipe(recipes.compiler(c_file, o_file, cflags))

  lake.target(d_file)
      :dependsOn(c_file)
      :recipe(recipes.depfile(c_file, d_file, o_file, cflags))
end

local ofiles = lake.flatten {reports.lake.objFiles, reports.iro.objFiles}

lakeexe
  :dependsOn(ofiles)
  :recipe(recipes.linker(ofiles, lakeexe, lflags))

options.getProjLibs("luajit"):each(function(e)
  lakeexe:dependsOn(e)
end)

