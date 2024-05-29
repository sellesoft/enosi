local options = assert(lake.getOptions())

local Twine = require "twine"
local List = require "list"

local mode = options.mode or "debug"
local build_dir = lake.cwd().."/build/"..mode.."/"

options.registerCleaner(function()
	lake.rm(build_dir, { recursive = true, force = options.force_clean })
end)

lake.mkdir(build_dir, {make_parents = true})

local report  = assert(options.report)
local reports = assert(options.reports)
local recipes = assert(options.recipes)

options.assertImported("iro", "llvm")

local c_files = lake.find("src/**/*.cpp")

local cflags = List
{
	options.getProjIncludeDirFlags("llvm", "iro"),
}

local lflags = List
{
	"-static",
	options.getProjLibDirFlags "llvm",

	-- TODO(sushi) get rid of this group stuff and just figure out the proper link ordering
	"-Wl,--start-group",
	options.getProjLibFlags "llvm",
	"-Wl,--end-group"
}

local lib = lake.target(build_dir.."liblppclang.a")

report.libDir(build_dir)
report.lib("lppclang")

c_files:each(function(c_file)
	local o_file = c_file:gsub("(.-)%.cpp", build_dir.."%1.o")
	local d_file = o_file:gsub("(.-)%.o", "%1.d")

	report.objFile(o_file)

	lib:dependsOn(o_file)

	lake.target(o_file)
		:dependsOn(c_file)
		:recipe(recipes.compiler(c_file, o_file, cflags))

	lake.target(d_file)
		:dependsOn(c_file)
		:recipe(recipes.depfile(c_file, d_file, o_file, cflags))
end)

lib:recipe(recipes.staticLib(reports.lppclang.objFiles, lib))
