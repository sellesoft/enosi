--
--
--  Lake module used in lake files.
-- 
--  This is loaded by lake in lake.cpp.
--
--

local errhandler = function(message)
	print(debug.traceback())
	io.write("err: ", tostring(message), "\n")
	return message
end

-- NOTE(sushi) hardcoded access to lua files defined in lake's source tree when im running 
--             in debug from enosi's root. This shouldn't affect release builds since it 
--             will be compiled in.
package.path = package.path..";./lake/src/?.lua"

local List = require "list"
local Twine = require "twine"

local lake = {}

-- internal target map
local targets = {}
local recipe_table = { next = 1 }

-- used for referencing things defined later in this file
-- as i dont really know of any other good way to do it 
local forward = {}

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

	void* lua__createSingleTarget(str path);
	void  lua__makeDep(void* target, void* dependent);
	s32   lua__targetSetRecipe(void* target);
	void* lua__createGroupTarget();
	void  lua__addTargetToGroup(void* group, void* target);
	b8    lua__targetAlreadyInGroup(void* group);
	void  lua__stackDump();
	u64   lua__getMonotonicClock();
	b8    lua__makeDir(str path, b8 make_parents);

	const char* lua__nextCliarg();

	void* lua__processSpawn(str* args, u32 args_count);
	void lua__processRead(
		void* proc, 
		void* stdout_ptr, u64 stdout_len, u64* out_stdout_bytes_read,
		void* stderr_ptr, u64 stderr_len, u64* out_stderr_bytes_read);
	b8 lua__processPoll(void* proc, s32* out_exit_code);
	void lua__processClose(void* proc);

	typedef struct
	{
		str* paths;
		s32 paths_count;
	} LuaGlobResult;

	LuaGlobResult lua__globCreate(str pattern);
	void lua__globDestroy(LuaGlobResult x);

	void lua__setMaxJobs(s32 n);

	str lua__getTargetPath(void* handle);
]]
local C = ffi.C
local strtype = ffi.typeof("str")

local make_str = function(s) 
	if not s then
		print(debug.traceback())
	end
	return strtype(s, #s) 
end


-- * << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << -
--      
--      Module initialization
--
-- * >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> -


-- * : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : : command line arguments
--   Process command line arguments.
while true do
	local arg = C.lua__nextCliarg()

	if arg == nil then
		break
	end

	arg = ffi.string(arg)

	local var, value = arg:match("(%w+)=(%w+)")

	if not var then
		-- this is assumed to be a target the user wants to make specifically.
		-- currently i do not support this because i dont have a usecase for it yet 
		-- and im not sure how i would integrate that functionality wih how the 
		-- build system currently works, so we'll throw an error about it for now
		error("arg '"..arg.."' is not a variable assignment nor a known option. It is assumed that this is a specific target you want to be made (eg. the same behavior as make), but this is not currently supported!")
	end

	lake.clivars[var] = value
end



-- * << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << -
--      
--      Lake util functions
--
--      Utility api provided to lake files for finding files, replacing strings, etc. as well
--      as functions used to support lake's syntax sugar, such as 'concat' which is used 
--      to support the '..=' operator.
--
-- * >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> -



-- * ---------------------------------------------------------------------------------------------- lake.canonicalizePath
-- | Returns a canonical representation of the given path or nil if one cannot be made. This 
-- | requires a path to an existing file.
lake.canonicalizePath = function(path)
	return lua__canonicalizePath(path)
end
-- |
-- * ----------------------------------------------------------------------------------------------

-- * ---------------------------------------------------------------------------------------------- lake.flatten
-- | Flattens nested array elements of the given table into a new one.
lake.flatten = function(tbl)
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

-- * ---------------------------------------------------------------------------------------------- lake.import
-- | 
local options_stack = {}
lake.import = function(s, options)
	table.insert(options_stack, options)
	local results = {lua__importFile(s)}
	table.remove(options_stack, #options_stack)
	return table.unpack(results)
end
-- |
-- * ----------------------------------------------------------------------------------------------

-- * ---------------------------------------------------------------------------------------------- lake.getOptions
-- | 
lake.getOptions = function()
	return options_stack[#options_stack]
end
-- |
-- * ----------------------------------------------------------------------------------------------

-- * ---------------------------------------------------------------------------------------------- lake.cwd
-- | 
lake.cwd = function()
	return lua__cwd()
end
-- |
-- * ----------------------------------------------------------------------------------------------

-- * ---------------------------------------------------------------------------------------------- lake.maxjobs
-- | Attempt to set the max jobs lake will use to build targets. If this number is set on command
-- | line, then this call is ignored.
lake.maxjobs = function(n)
	C.lua__setMaxJobs(n)
end
-- |
-- * ----------------------------------------------------------------------------------------------

-- * ---------------------------------------------------------------------------------------------- lake.mkdir
-- | Creates a directory at 'path'. Optionally takes a table of 'options', which are:
-- |   
-- |   make_parents: if true, then mkdir will also create any missing parent directories in the 
-- |                 given path.
-- |
-- | Returns true if the directory at the path was created.
lake.mkdir = function(path, options)
	if type(path) ~= "string" then
		error("path given to lake.mkdir is not a string! it is "..type(path), 2)
	end

	local make_parents = false

	if options then
		if options.make_parents then
			make_parents = true
		end
	end

	if C.lua__makeDir(strtype(path, #path), make_parents) == 0 then
		return false
	end

	return true
end
-- |
-- * ----------------------------------------------------------------------------------------------

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

	local glob = C.lua__globCreate(make_str(pattern))

	if not glob.paths then
		return {}
	end

	local out = {}

	for i=0,tonumber(glob.paths_count-1) do
		local s = glob.paths[i]
		table.insert(out, ffi.string(s.s, s.len))
	end

	C.lua__globDestroy(glob)

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
-- | Strings and targets may both be passed. If a target is passed it is resolved to its path. If 
-- | the target is a group an error is thrown for now as I am not sure yet how that should be 
-- | handled. Tables of both of these things may be passed and those tables may also contain 
-- | tables of all of these things recursively. It will be resolved into a flat array of strings
-- | internally.
-- |
-- | TODO(sushi) this has been made very ugly in a fit of trying to get stuff to work again
-- |             please clean it up soon
lake.cmd = function(...)
	local args = List()

	local function recur(x)
		for _,v in ipairs(x) do
			local vtype = type(v)

			if "string" == vtype then
				args:push(v)
			elseif "table" == vtype then
				if forward.TargetGroup == getmetatable(v) then
					return false, "flattened argument given to lake.cmd resolves to a target group, which is not currently allowed. You must manually resolve this to a list of target paths."
				end

				if forward.Target == getmetatable(v) then
					args:push(v.path)
				elseif List == getmetatable(v) then
					for s in v:each() do
						local success, message = recur(s)
						if not success then
							return false, message
						end
					end
				elseif Twine == getmetatable(v) then
					for s in v:each() do
						args:push(s)
					end
				else
					local success, message = recur(v)
					if not success then
						return false, message
					end
				end
			else
				return false, "flattened argument given to lake.cmd was not resolvable to a string. Its value is "..tostring(v)
			end
		end

		return true
	end

	local success, message = recur{...}

	if not success then
		error(message, 2)
	end

	local argsarr = ffi.new("str["..(args:len()+1).."]")

	for arg,i in args:eachWithIndex() do
		if "string" ~= type(arg) then
			local errstr = "argument "..tostring(i).." in call to cmd was not a string, it was a "..type(arg)..". Arguments were:\n"

			for _,arg in ipairs(args) do
				errstr = " "..errstr..arg.."\n"
			end

			error(errstr)
		end

		argsarr[i-1] = make_str(args[i])
	end

	local handle = C.lua__processSpawn(argsarr, args:len()+1)

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

	local exit_code = ffi.new("s32[1]")
	local out_read = ffi.new("u64[1]")
	local err_read = ffi.new("u64[1]")

	local doRead = function()
		local out_ptr, out_len = out_buf:reserve(space_wanted)
		local err_ptr, err_len = err_buf:reserve(space_wanted)

		C.lua__processRead(handle,
			out_ptr, out_len, out_read,
			err_ptr, err_len, err_read)

		-- print(out_read[0], err_read[0])

		out_buf:commit(out_read[0])
		err_buf:commit(err_read[0])

		return out_read[0] + err_read[0]
	end

	while true do
		doRead()

		local ret = C.lua__processPoll(handle, exit_code)

		if ret ~= 0 then
			-- try to read until we stop recieving data because the process can report
			-- that its terminated before all of its buffered output is consumed
			while doRead() ~= 0 do end

			C.lua__processClose(handle)

			return
			{
				exit_code = exit_code[0],
				stdout = out_buf:get(),
				stderr = err_buf:get()
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
		local out = List()
		for _,v in ipairs(subject) do
			if type(v) ~= "string" then
				error("element of table given to lake.replace is not a string!", 2)
			end

			local res = v:gsub(search, repl)

			out:insert(res)
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
				lhs:insert(v)
			end
			return lhs
		elseif rhs_type == "string" then
			lhs:insert(rhs)
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
	if type(t1) ~= "table" then
		error("first argument passed to lake.zip is not a table! it is "..type(t1), 2)
	end

	if type(t2) ~= "table" then
		error("second argument passed to lake.zip is not a table! it is "..type(t2), 2)
	end

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
	return tonumber(C.lua__getMonotonicClock())
end
-- |
-- * ----------------------------------------------------------------------------------------------


-- * << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << -
--      
--      Target api
--
-- * >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> -


local Target =
{
	path = "** target with no path! **",

	-- handle to lake's internal representation
	handle = nil,
}
Target.__index = Target
forward.Target = Target

-- * ---------------------------------------------------------------------------------------------- Target.new
-- | Create a new Target.
Target.new = function(path)
	local o = {}
	setmetatable(o, Target)
	o.path = path
	o.handle = C.lua__createSingleTarget(make_str(path))
	return o
end
-- |
-- * ----------------------------------------------------------------------------------------------

-- * ---------------------------------------------------------------------------------------------- Target.__tostring
-- | Targets return their path in string contexts.
Target.__tostring = function(self)
	return self.path
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
		C.lua__makeDep(self.handle, lake.target(x).handle)
	elseif "table" == x_type then
		local flat = flatten_table(x)
		for i,v in ipairs(flat) do
			local v_type = type(v)
			if "string" ~= v_type then
				error("Element "..i.." of flattened table given to Target.uses is not a string, rather a "..v_type.." whose value is "..tostring(v)..".", 2)
			end
			C.lua__makeDep(self.handle, lake.target(v).handle)
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
		C.lua__makeDep(self.handle, lake.target(x).handle)
	elseif "table" == x_type then
		for i,v in ipairs(lake.flatten(x)) do
			local v_type = type(v)
			if "string" ~= v_type then
				error("Element "..i.." of flattened table given to Target.depends_on is not a string, rather a "..v_type.." whose value is "..tostring(v)..".", 2)
			end
			C.lua__makeDep(self.handle, lake.target(v).handle)
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
	local recipeidx = C.lua__targetSetRecipe(self.handle)
	recipe_table[recipeidx] = co.create(f)
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
forward.TargetGroup = TargetGroup

-- * ---------------------------------------------------------------------------------------------- TargetGroup.new
-- | Create a new TargetGroup.
TargetGroup.new = function()
	local o = {}
	setmetatable(o, TargetGroup)
	o.targets = List()
	o.handle = C.lua__createGroupTarget()
	return o
end
-- |
-- * ----------------------------------------------------------------------------------------------

-- * ---------------------------------------------------------------------------------------------- TargetGroup:uses
-- | Calls 'uses' for every target in this group.
TargetGroup.uses = function(self, x)
	for target in self.targets:each() do
		target:uses(x)
	end
	return self
end
-- |
-- * ----------------------------------------------------------------------------------------------

-- * ---------------------------------------------------------------------------------------------- TargetGroup:depends_on
-- | Calls 'depends_on' for every target in this group.
TargetGroup.depends_on = function(self, x)
	local x_type = type(x)
	if "string" == x_type then
		C.lua__makeDep(self.handle, lake.target(x).handle)
	elseif "table" == x_type then
		local flat = flatten_table(x)
		for i,v in ipairs(flat) do
			local v_type = type(v)
			if "string" ~= v_type then
				error("Element "..i.." of flattened table given to TargetGroup.depends_on is not a string, rather a "..v_type.." whose value is "..tostring(v)..".", 2)
			end
			C.lua__makeDep(self.handle, lake.target(v).handle)
		end
	else
		error("TargetGroup.depends_on can take either a string or a table of strings, got: "..x_type..".", 2)
	end
	return self
end
-- |
-- * ----------------------------------------------------------------------------------------------

-- * ---------------------------------------------------------------------------------------------- TargetGroup:recipe
-- | Sets the recipe for this group.
TargetGroup.recipe = function(self, f)
	local recipeidx = C.lua__targetSetRecipe(self.handle)
	recipe_table[recipeidx] = co.create(f)
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
	local args = List{...}

	local group = TargetGroup.new()

	for arg,i in args:eachWithIndex() do
		if type(arg) ~= "string" then
			error("flattened argument "..i.." given to lake.targets was not a string, rather a "..type(arg).." with value "..tostring(arg),2)
		end

		local target = lake.target(arg)

		if target.recipe_fn then
			error("created target '"..target.path.."' for group, but this target already has a recipe!",2)
		end

		if C.lua__targetAlreadyInGroup(target.handle) ~= 0 then
			error("target '"..target.path.."' is already in a group!",2)
		end

		C.lua__addTargetToGroup(group.handle, target.handle)

		group.targets:insert(target)
	end

	return group
end

return lake, targets, recipe_table, co.resume, errhandler
