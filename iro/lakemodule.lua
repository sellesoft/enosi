local options = assert(lake.getOptions())

local Twine = require "twine"
local List = require "list"

local mode = options.mode or "debug"
local build_dir = lake.cwd().."/build/"..mode.."/"

options.registerCleaner(function()
	lake.rm(build_dir, {recursive = true, force = options.force_clean})
end)

lake.mkdir(build_dir, {make_parents = true})

local report = assert(options.report)
local recipes = assert(options.recipes)

report.includeDir(lake.cwd())

local cflags = Twine.new
	"-fvisibility=hidden" -- prevent bloating the dynamic symbol table, as most of my projects 
						  -- currently need to export all symbols so luajit may use them

if mode == "debug" then
	cflags "-O0" "-ggdb3" "-DIRO_DEBUG=1"
else
	cflags "-O2"
end

if lake.os() == "Linux" then
	cflags "-DIRO_LINUX=1"
end

local c_files = lake.find("iro/**/*.cpp")

for c_file in c_files:each() do
	local o_file = c_file:gsub("(.-)%.cpp", build_dir.."%1.o")
	local d_file = o_file:gsub("(.-)%.o", "%1.d")

	report.objFile(o_file)

	lake.target(o_file) -- TODO(sushi) consider using groups to make this_file 
	                    --             dependency not need to be applied to 
						--             every file
		:dependsOn {c_file, options.this_file} 
		:recipe(recipes.compiler(c_file, o_file, cflags))

	lake.target(d_file)
		:dependsOn(c_file)
		:recipe(recipes.depfile(c_file, d_file, o_file, cflags))
end

local reset = "\027[0m"
local green = "\027[0;32m"
local blue  = "\027[0;34m"
local red   = "\027[0;31m"

-- Build lua object files so they can be statically linked
-- as this simplifies using lua libraries greatly.
--
-- TODO(sushi) report these object files in a different 
--             category so that projects that dont 
--             need lua stuff dont link them
lake.find("iro/lua/*.lua"):each(function(lua_file)
	local o_file = lua_file:gsub("(.-)%.lua", build_dir.."%1.lua.o")
	report.objFile(o_file)

	lake.target(o_file)
		:dependsOn { lua_file, options.this_file }
		:recipe(recipes.luaToObj(lua_file, o_file))
end)
