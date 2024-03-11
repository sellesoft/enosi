--
--
--  Lake module used in lake files.
-- 
--  This is loaded by lake in lake.cpp.
--
--

local lake = {}
local lake_internal = {}

-- internal target map
local targets = {}

-- forward decls
local Target

-- table for storing vars specified on the command line
-- to support the '?=' syntax. 
--   Ex. 
--      if in the lakefile we have
--
--   		mode ?= "debug"
--
--   	the line will be transformed into
--         
--          local mode = lake.cliargs.mode or "debug"
--
--      and if someone invokes lake as
--
--      	lake mode=release
--
--     mode will be "release".
--
lake.clivars = {}


-- * << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << -
--      
--      C interface
--
-- * >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> -


local ffi = require "ffi"
ffi.cdef [[
	typedef uint8_t  u8;
	typedef uint16_t u16;
	typedef uint32_t u32;
	typedef uint64_t u64;
	typedef int8_t   s8;
	typedef int16_t  s16;
	typedef int32_t  s32;
	typedef int64_t  s64;
	typedef float    f32;
	typedef double   f64;
	typedef u8       b8; // booean type

	typedef struct str
	{
		const char* s;
		s32 len;
	} str;

	u8* read_files(str path);

	typedef void* DirIter;

	DirIter dir_iter(const char* path);
	u32     dir_next(char* c, u32 maxlen, DirIter iter);

	typedef struct Glob 
	{
		s64 n_paths;
		u8** paths;

		void* handle;
	} Glob;

	Glob glob_create(const char* pattern);
	void glob_destroy(Glob glob);

	b8 is_file(const char* path);
	b8 is_dir(const char* path);

	s64 modtime(const char* path);

	typedef struct Target Target;

	Target* lua__create_target(str path);
	void    lua__make_dep(Target* target, Target* dependent);

	typedef struct CLIargs
	{
		s32 argc;
		const char** argv;
	} CLIargs;

	CLIargs lua__get_cliargs();
]]
local C = ffi.C
local strtype = ffi.typeof("str")


-- * << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << -
--      
--      Module initialization
--
-- * >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> -


-- * : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : command line arguments
--   Process command line arguments.
local args = C.lua__get_cliargs()

print(args.argc)

for i=1,args.argc-1 do
	local arg = ffi.string(args.argv[i])
	
	local var, value = arg:match("(%w+)=(%w+)")
	
	if not var then
		error("arg "..i.." could not be resolved: "..arg)
	end
	
	lake.clivars[var] = value

	print(var, " ", value)
end


-- * << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << -
--      
--      Internal helpers
--
-- * >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> -


-- * ---------------------------------------------------------------------------------------------- flatten_table
-- | Flattens nested array elements of the given table into a new one.
local flatten_table = function(tbl)
	local out = {}

	function recur(x)
		for _,v in ipairs(tbl) do
			if type(v) == "table" then
				recur(v)
			else
				table.insert(out, v)
			end
		end
	end

	recur(tbl)
	return out
end
-- |
-- * ----------------------------------------------------------------------------------------------


-- * << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << -
--      
--      Lake util functions
--
--      Utility api provided to lake files for finding files, replacing strings, etc. as well
--      as functions used to support lake's syntax sugar, such as 'concat' which is used 
--      to support the '..=' operator.
--
-- * >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> -


-- * ---------------------------------------------------------------------------------------------- lake.find
-- | Performs a shell glob using the given pattern and returns a list of resulting strings.
-- |
-- | Ex. 
-- | 	c_files := lake.find("src/*.cpp")
-- |
-- |	--> src/lake.cpp
-- |	    src/graph.cpp
-- |	    ...
lake.find = function(pattern)
	if type(pattern) ~= "string" then
		error("pattern given to lake.find must be a string!", 2)
	end

	local glob = C.glob_create(pattern)

	if not glob.handle then
		return {}
	end

	local out = {}

	for i=0,tonumber(glob.n_paths-1) do
		table.insert(out, ffi.string(glob.paths[i]))
	end

	return out
end
-- |
-- * ----------------------------------------------------------------------------------------------

-- * ---------------------------------------------------------------------------------------------- lake.replace
-- | Performs a string replacement on the given 'subject', which may be a string or a table of 
-- | strings. When given a table of strings a new table will be returned with each result.
-- |
-- | This is really a wrapper over lua's gsub so we can support tables. For details on how to use
-- | lua's patterns see: 
-- | 	https://www.lua.org/manual/5.1/manual.html#pdf-string.gsub
-- |
-- | Ex. 
-- | 	c_files := lake.find("src/*.cpp")
-- | 	o_files := lake.replace(c_files, "src/(.-).cpp", "build/$1.o")
-- |
-- | 	--> build/lake.o
-- | 		build/graph.o
-- | 		...
lake.replace = function(subject, search, repl)
	if type(subject) == "table" then
		local out = {}
		for _,v in ipairs(subject) do
			if type(v) ~= "string" then
				error("element of table given to lake.replace is not a string!", 2)
			end

			local res = v:gsub(search, repl)

			table.insert(out, res)
		end
		return out
	else
		error("unsupported type given as subject of lake.replace: '"..type(subject).."'", 2)
	end
end
-- |
-- * ----------------------------------------------------------------------------------------------

lake.concat = function(lhs, rhs)
	local lhs_type = type(lhs)
	local rhs_type = type(rhs)
	if lhs_type == "table" then
		if rhs_type == "table" then
			for _,v in ipairs(rhs) do
				table.insert(lhs, v)
			end
			return lhs
		elseif rhs_type == "string" then
			table.insert(lhs, rhs)
			return lhs
		else
			error("unsupported type combination given to lake.concat: "..lhs_type..", "..rhs_type, 2)
		end
	elseif lhs_type == "string" then
		if rhs_type == "string" then
			return rhs..lhs
		else
			error("unsupported type combination given to lake.concat: "..lhs_type..", "..rhs_type, 2)
		end
	else
		error("unsupported type given as lhs of lake.concat: '"..lhs_type.."'", 2)
	end
end

lake.zip = function(t1, t2)
	local t1l = #t1
	local t2l = #t2

	if t1l ~= t2l then
		error("lake.zip expected the given tables to be of the same length! Table 1 has "..t1l.." elements and table 2 has "..t2l.." elements", 2)
	end

	local i = 0

	local iter = function()
		i = i + 1
		if i <= t1l then
			return t1[i], t2[i]
		end
	end

	return iter
end


-- * << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << -
--      
--      Target api
--
-- * >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> -


local Target = {
	path = "** target with no path! **",

	-- handle to lake's internal representation
	handle = nil, 

	-- lua function used to build the target
	recipe_fn = nil,
	
	-- array of targets this target uses directly
	uses_targets = nil,

	-- array of targets this target depends on but does not use
	depends_on_targets = nil,
}
Target.__index = Target

-- * ---------------------------------------------------------------------------------------------- Target.new
-- | Create a new Target.
Target.new = function(path)
	local o = {}
	setmetatable(o, Target)
	o.path = path
	o.handle = C.lua__create_target(strtype(path, #path))
	o.uses_targets = {}
	o.depends_on_targets = {}
	return o
end
-- |
-- * ----------------------------------------------------------------------------------------------

-- * ---------------------------------------------------------------------------------------------- Target:uses
-- | Defines a target or targets that this target directly uses in its recipe. 
-- | All targets provided here will be passed to the 'inputs' argument of the targets recipe.
-- | May be a string or a table of strings.
Target.uses = function(self, x)
	local x_type = type(x)
	if "string" == x_type then
		C.lua__make_dep(self.handle, lake.target(x).handle)
	elseif "table" == x_type then
		local flat = flatten_table(x)
		for i,v in ipairs(flat) do
			local v_type = type(v)
			if "string" ~= v_type then
				error("Element "..i.." of flattened table given to Target.uses is not a string, rather a "..v_type..".", 2)
			end
			C.lua__make_dep(self.handle, lake.target(v).handle)
		end
	else
		error("Target.uses can take either a string or a table of strings, got: "..x_type..".", 2)
	end
	return self
end
-- |
-- * ----------------------------------------------------------------------------------------------

-- * ---------------------------------------------------------------------------------------------- Target:recipe
-- | Defines the recipe a target uses to create its file.
-- | Must be a lua function.
Target.recipe = function(self, f)
	if "function" ~= type(f) then
		error("expected a lua function as the recipe of target '"..self.path.."', got: "..type(f), 2)
	end
	io.write("setting recipe of target '"..self.path.."' to "..tostring(f).."\n")
	self.recipe_fn = f
	return self
end
-- |
-- * ----------------------------------------------------------------------------------------------

-- * ---------------------------------------------------------------------------------------------- lake.target
-- | Get the Target object representing 'path'. If the target has not been registered it will be
-- | created, otherwise the already existing target is returned.
lake.target = function(path)
	local target = targets[path]
	if target then
		return target
	end
	targets[path] = Target.new(path)
	return targets[path]
end
-- |
-- * ----------------------------------------------------------------------------------------------


-- * << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << -
--      
--      Internal functions probably mostly called by C
--
-- * >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> -


-- * ---------------------------------------------------------------------------------------------- lake_internal.run_recipe
-- | Given a target's path this function will attempt to run its recipe. If it fails for some 
-- | reason then an error is thrown. Otherwise returns the result of the recipe function.
lake_internal.run_recipe = function(path)
	local target = targets[path]

	if not target then
		error("no target associated with path '"..path.."'")
	end

	if not target.recipe_fn then
		error("target '"..path.."' does not define a recipe")
	end
end
-- |
-- * ----------------------------------------------------------------------------------------------

return lake, lake_internal
