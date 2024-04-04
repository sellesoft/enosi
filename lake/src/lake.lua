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

-- used for asyncronously running commands in recipes
local co = require "coroutine"


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
	typedef struct TargetGroup TargetGroup;

	Target*      lua__create_target(str path);
	void         lua__make_dep(Target* target, Target* dependent);
	void         lua__target_set_has_recipe(Target* target);
	TargetGroup* lua__create_group();
	void         lua__add_target_to_group(TargetGroup* group, Target* target);
	b8           lua__target_already_in_group(Target* group);

	typedef struct CLIargs
	{
		s32 argc;
		const char** argv;
	} CLIargs;

	CLIargs lua__get_cliargs();

	void* process_spawn(const char** args);

	typedef struct PollReturn
	{
		int out_bytes_written;
		int err_bytes_written;
	} PollReturn;

	typedef void (*exit_cb_t)(int);

	PollReturn process_poll(
		void* proc,
		void* stdout_dest, int stdout_suggested_bytes_read, 
		void* stderr_dest, int stderr_suggested_bytes_read,
		exit_cb_t on_exit);

	u64 get_highres_clock();
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
		for _,v in ipairs(x) do
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

-- * ---------------------------------------------------------------------------------------------- lake.cmd
-- | Takes a variable amount of arguments all of which are expected to be strings or arrays of 
-- | strings which will be flattened to form a command to run. The first string is the name of the
-- | process and what follows are arguments to be passed to the process.
-- |
-- | This can be used directly, but it meant to be what the command syntax sugar is transformed 
-- | into. For example:
-- |
-- |   result := `gcc -c main.c -o main.c`
-- | 
-- | is transformed into
-- | 
-- |   local result = lake.cmd("gcc", "-c", "main.c", "-o", "main.c")
-- | 
-- | Arrays are flattened recursively so the user does not have to worry about doing this themself.
lake.cmd = function(...)
	local args = flatten_table{...}

	local argsarr = ffi.new("const char*["..(#args+1).."]")

	for i,arg in ipairs(args) do
		if "string" ~= type(arg) then
			local errstr = "argument "..tostring(i).." in call to cmd was not a string, it was a "..type(arg)..". Arguments were:\n"

			for _,arg in ipairs(args) do
				errstr = " "..errstr..arg.."\n"
			end

			error(errstr)
		end

		argsarr[i-1] = ffi.new("const char*", args[i])
	end

	--local s = ""
	--for _,arg in ipairs(args) do
	--	s = s..arg.." "
	--end
	--print(s)

	local handle = C.process_spawn(argsarr)

	if not handle then
		local argsstr = ""
		for _,arg in ipairs(args) do
			argsstr = " "..argsstr..arg.."\n"
		end
		error("failed to spawn process with arguments:\n"..argsstr)
	end

	local buffer = require "string.buffer"

	local out_buf = buffer.new()
	local err_buf = buffer.new()

	local space_wanted = 256

	local exit_status = nil

	-- cache this somewhere better later
	-- this is also kind of stupid, only doing it cause its just how i 
	-- did it long ago in the first lua build sys attempt. The poll 
	-- function should just return the state of the process.
	local on_close = function(exit_stat)
		exit_status = exit_stat
	end

	local c_callback = ffi.cast("exit_cb_t", on_close)

	while true do
		local out_ptr, out_len = out_buf:reserve(space_wanted)
		local err_ptr, err_len = err_buf:reserve(space_wanted)

		local ret =
			C.process_poll(
				handle,
				out_ptr, out_len,
				err_ptr, err_len,
				c_callback)

		out_buf:commit(ret.out_bytes_written)
		err_buf:commit(ret.err_bytes_written)

		if exit_status then
			c_callback:free()
			return {
				exit_status = exit_status,
				stdout = out_buf,
				stderr = err_buf,
			}
		else
			co.yield(false)
		end
	end
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
	elseif type(subject) == "string" then
		local res = subject:gsub(search, repl)
		return res
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


-- * ---------------------------------------------------------------------------------------------- lake.get_highres_clock
-- | Returns a highres clock in microseconds.
lake.get_highres_clock = function()
	return tonumber(C.get_highres_clock())
end
-- |
-- * ----------------------------------------------------------------------------------------------


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
-- |
-- | TODO(sushi) currently this assumes the same target wont be passed more than once, which 
-- |             we need to handle somehow. We could probably just generate an array of 
-- |             target paths this one depends on from the internal TargetSet just before
-- |             we run the target's recipe.
-- |             It also uses a name on the Target object itself which I would like to avoid.
-- |
-- | TODO(sushi) Make this variadic
Target.uses = function(self, x)
	local x_type = type(x)
	if "string" == x_type then
		C.lua__make_dep(self.handle, lake.target(x).handle)
		table.insert(self.uses_targets, x)
	elseif "table" == x_type then
		local flat = flatten_table(x)
		for i,v in ipairs(flat) do
			local v_type = type(v)
			if "string" ~= v_type then
				error("Element "..i.." of flattened table given to Target.uses is not a string, rather a "..v_type.." whose value is "..tostring(v)..".", 2)
			end
			C.lua__make_dep(self.handle, lake.target(v).handle)
			table.insert(self.uses_targets, v)
		end
	else
		error("Target.uses can take either a string or a table of strings, got: "..x_type..".", 2)
	end
	return self
end
-- |
-- * ----------------------------------------------------------------------------------------------

-- * ---------------------------------------------------------------------------------------------- Target:depends_on
-- | Defines a target that this target depends on, but does not use in its inputs.
-- |
-- | TODO(sushi) Make this variadic
Target.depends_on = function(self, x)
	local x_type = type(x)
	if "string" == x_type then
		C.lua__make_dep(self.handle, lake.target(x).handle)
		table.insert(self.depends_on_targets, x)
	elseif "table" == x_type then
		local flat = flatten_table(x)
		for i,v in ipairs(flat) do
			local v_type = type(v)
			if "string" ~= v_type then
				error("Element "..i.." of flattened table given to Target.depends_on is not a string, rather a "..v_type.." whose value is "..tostring(v)..".", 2)
			end
			C.lua__make_dep(self.handle, lake.target(v).handle)
			table.insert(self.depends_on_targets, v)
		end
	else
		error("Target.depends_on can take either a string or a table of strings, got: "..x_type..".", 2)
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
	self.recipe_fn = co.create(f)
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

-- A grouping of targets that are built from a single invocation of a common recipe.
local TargetGroup =
{
	-- array of targets belonging to this group
	targets = nil,

	-- handle to C representation of the group
	handle = nil,
}
TargetGroup.__index = TargetGroup

-- * ---------------------------------------------------------------------------------------------- TargetGroup.new
-- | Create a new TargetGroup.
TargetGroup.new = function()
	local o = {}
	setmetatable(o, TargetGroup)
	o.targets = {}
	o.handle = C.lua__create_group()
	return o
end
-- |
-- * ----------------------------------------------------------------------------------------------

-- * ---------------------------------------------------------------------------------------------- TargetGroup:uses
-- | Calls 'uses' for every target in this group.
-- |
-- | TODO(sushi) if for whatever reason there is an error in Target.uses it will report that it 
-- |             came from that function, not here, which could be confusing, so fix that 
-- |             eventually.
TargetGroup.uses = function(self, x)
	for _,target in ipairs(self.targets) do
		target:uses(x)
	end
	return self
end
-- |
-- * ----------------------------------------------------------------------------------------------

-- * ---------------------------------------------------------------------------------------------- TargetGroup:depends_on
-- | Calls 'depends_on' for every target in this group.
-- |
-- | TODO(sushi) if for whatever reason there is an error in Target.depends_on it will report that 
-- |             it came from that function, not here, which could be confusing, so fix that 
-- |             eventually.
TargetGroup.depends_on = function(self, x)
	for _,target in ipairs(self.targets) do
		target:depends_on(x)
	end
	return self
end
-- |
-- * ----------------------------------------------------------------------------------------------

-- * ---------------------------------------------------------------------------------------------- TargetGroup:recipe
-- | Adds the same recipe to all targets.
TargetGroup.recipe = function(self, f)
	for _,target in ipairs(self.targets) do
		target:recipe(f)
	end
	return self
end
-- |
-- * ----------------------------------------------------------------------------------------------

-- * ---------------------------------------------------------------------------------------------- lake.targets
-- | Creates a grouping of targets, all of which use and depend on the same targets and which 
-- | all are produced by a single invocation of the given recipe.
-- | 
-- | The arguments passed to this function may be strings or tables of strings. Already existing 
-- | targets *cannot* be added to groups. 
-- | 
-- | Unlike 'target', every call to this function creates a new group! So if you want to create a 
-- | group and refer to it later, you must put it into a variable, for example:
-- |   
-- |   a := lake.targets("apple", "banana")
-- |   b := lake.targets("apple", "banana")
-- | 
-- | 'a' and 'b' refer to different groups, and the second call to 'targets' will throw an 
-- | error, as no target may belong to two different groups!
lake.targets = function(...)
	local args = flatten_table{...}

	local group = TargetGroup.new()

	for i,arg in ipairs(args) do
		if type(arg) ~= "string" then
			error("flattened argument "..i.." given to lake.targets was not a string, rather a "..type(arg).." with value "..tostring(arg),2)
		end

		local target = lake.target(arg)

		if target.recipe_fn then
			error("created target '"..target.path.."' for group, but this target already has a recipe!",2)
		end

		if C.lua__target_already_in_group(target.handle) ~= 0 then
			error("target '"..target.path.."' is already in a group!",2)
		end

		C.lua__add_target_to_group(group.handle, target.handle)

		table.insert(group.targets, target)
	end

	return group
end


-- * << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << -
--      
--      Internal functions probably mostly called by C
--
-- * >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> -


-- * ---------------------------------------------------------------------------------------------- lake_internal.resume_recipe
-- | Given a target's path this function will resume its recipe coroutine.
-- | If the recipe is finished true is returned.
lake_internal.resume_recipe = function(path)
	local target = targets[path]

	if not target then
		error("no target associated with path '"..path.."'")
	end

	if not target.recipe_fn then
		error("target '"..path.."' does not define a recipe")
	end

	-- TODO(sushi) restructure this stuff so that we dont have to pass these two args everytime we resume
	local result = {co.resume(target.recipe_fn, target.uses_targets, target.path)}

	if not result[1] then
		error("encountered lua error while running recipe for target '"..path.."':\n"..result[2])
	end

	if result[2] == nil then
		return true
	end

	return false
end
-- |
-- * ----------------------------------------------------------------------------------------------

return lake, lake_internal, targets
