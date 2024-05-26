--
--
--  Lake module used in lake files.
-- 
--  This is loaded by lake in lake.cpp.
--
--

-- NOTE(sushi) hardcoded access to lua files defined in lake's source tree when im running 
--             in debug from enosi's root. This shouldn't affect release builds since it 
--             will be compiled in.
package.path = package.path..";./lake/src/?.lua"

local co = require "coroutine"
local List = require "list"
local Twine = require "twine"

local errhandler = function(message)
	print(debug.traceback())
	return message
end

--- Global lake table used to report targets and dependencies between them
--- and various helpers.
---
---@class lake
lake = {}

-- internal target map
local targets = {}
local recipe_table = { next = 1 }

local Target,
	  TargetGroup

--- Table containing variables specified on command line. 
--- For example if lake is invoked:
---
--- ```shell
--- 	lake mode=release
---	```
---
--- then this table will contain a key 'mode' equal to "release".
--- This is useful for a quick way to change a lakefile's behavior from
--- command line, an example of its use:
---
--- ```lua
---     local mode = lake.clivars.mode or "debug"
--- ```
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

	b8 lua__copyFile(str dst, str src);

	b8 lua__chdir(str path);

	b8 lua__unlinkFile(str path);
]]
local C = ffi.C
local strtype = ffi.typeof("str")

---@class str
---@field s cdata
---@field len number

---@param s string
---@return str
local make_str = function(s)
	if not s then
		print(debug.traceback())
		os.exit(1)
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



-- * ---------------------------------------------------------------------------------------------- lake.os

--- Get a string specifying the current operating system.
---
--- Currently this just returns whatever luajit reports from its os() function.
--- This could be one of: 
--- 	Windows
---     Linux
---     OSX
---     BSD
---     POSIX
---     Other
---
--- TODO(sushi) make this return the target operating system if I ever get around to supporting
---             cross-compilation.
---
---@return string
lake.os = function()
	return jit.os
end

-- * ---------------------------------------------------------------------------------------------- lake.arch

--- Get a string specifying the current architecture.
---
--- Currently this just returns whatever luajit reports from its arch() function.
--- This could be one of:
--- 	x86
--- 	x64
---		arm
---		arm64
---		arm64be
---		ppc
---		mips
---		mipsel
---		mips64
---		mips64el
---		mips64r6
---		mips64r6el
---
--- TODO(sushi) make this return the target architecture if I ever get around to supporting 
---             cross-compilation.
---
---@return string
lake.arch = function()
	return jit.arch
end

-- * ---------------------------------------------------------------------------------------------- lake.canonicalizePath

--- Takes a path to a file and returns a canonicalized/absolute
--- path to it. The file must exist!
---
---@param path string
---@return string
lake.canonicalizePath = function(path)
	return lua__canonicalizePath(path)
end

-- * ---------------------------------------------------------------------------------------------- lake.flatten

--- Takes a table, List or Twine, and flattens all nested tables,
--- Lists, Twines, etc. recursively and returns a new List of 
--- all elements. 
---
--- Note that for tables this only takes array 
--- elements, not key/values.
---
---@param x table|List|Twine
---@return List
lake.flatten = function(tbl)
	local out = List()

	local function recur(x)
		local xtype = type(x)

		if xtype == "table" then
			local xmt = getmetatable(x)

			if List == xmt then
				for e in x:each() do
					recur(e)
				end
			elseif Twine == xmt then
				for s in x:each() do
					out:push(s)
				end
			else
				for _,e in ipairs(x) do
					recur(e)
				end
			end
		else
			out:push(x)
		end
	end

	recur(tbl)
	return out
end

local options_stack = List()

-- * ---------------------------------------------------------------------------------------------- lake.import

--- Imports the lake module at 'path' and passes the table 'options'
--- to it. 'options' may be retrieved in the module by calling 
--- lake.getOptions().
--- 
--- This changes the cwd to the directory containing the given module
--- and any target that has its recipe set inside the module is assumed
--- to execute its recipe from that directory.
--- 
--- Returns anything the module file returns.
---
---@param path string
---@param options table?
---@return ...
lake.import = function(path, options)
	options_stack:push(options)
	local results = {lua__importFile(path)}
	options_stack:pop()
	return table.unpack(results)
end

-- * ---------------------------------------------------------------------------------------------- lake.getOptions

--- Gets the options set for the current module, or nil if not in a 
--- a module or if no options were passed.
---
---@return table?
lake.getOptions = function()
	return options_stack[options_stack:len()]
end

-- * ---------------------------------------------------------------------------------------------- lake.cwd

--- Returns the current working directory as a string
---
---@return string
lake.cwd = function()
	return lua__cwd()
end

-- * ---------------------------------------------------------------------------------------------- lake.chdir

--- Changes the current working directory to 'path'. Returns
--- a bool determining success.
---
---@param path string
---@return boolean
lake.chdir = function(path)
	return 0 ~= C.lua__chdir(make_str(path))
end

-- * ---------------------------------------------------------------------------------------------- lake.maxjobs

--- Attempt to set the max jobs lake will use in building. If this 
--- is set on command line (--max-jobs <n> or -j <n>), this call
--- is ignored.
---
---@param n number
lake.maxjobs = function(n)
	C.lua__setMaxJobs(n)
end

-- * ---------------------------------------------------------------------------------------------- lake.unlink

--- Unlinks (deletes) the file at 'path'. NOTE that this works on any kind of file!
--- 
--- Returns a bool determining success.
---
---@param path string
---@return boolean
lake.unlink = function(path)
	return 0 ~= C.lua__unlinkFile(make_str(path))
end

-- * ---------------------------------------------------------------------------------------------- lake.mkdir

--- Creates the directory at the given path.
--- 'options' is an optional table containing optional parameters
---		* make_parents | bool = false
---         Create any missing directories in the given path
---
---@param path string
---@param options table?
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

-- * ---------------------------------------------------------------------------------------------- lake.copy

--- Copies the file at 'src' to 'dst'.
---
---@param src string
---@param dst string
---@return boolean
lake.copy = function(dst, src)
	return C.lua__copyFile(make_str(dst), make_str(src))
end

-- * ---------------------------------------------------------------------------------------------- lake.find

--- Globs from the current working directory and returns a List 
--- of all files that match the given pattern.
---
--- The patterns supported are currently a subset of zsh globbing
--- and the implementation is directly based on Crystal's.
---
--- *, **, and ? are supported. Character classes [] and braces
--- {} are not, but might be in the future.
---
---@param pattern string
---@return List 
lake.find = function(pattern)
	if type(pattern) ~= "string" then
		error("pattern given to lake.find must be a string!", 2)
	end

	local glob = C.lua__globCreate(make_str(pattern))

	if not glob.paths then
		return {}
	end

	local out = List()

	for i=0,tonumber(glob.paths_count-1) do
		local s = glob.paths[i]
		out:push(ffi.string(s.s, s.len))
	end

	C.lua__globDestroy(glob)

	return out
end

-- * ---------------------------------------------------------------------------------------------- lake.cmd

--- Information about the result of a call to lake.cmd.
---@class CmdResult
---
--- The exit code reported by the program on exit.
---@field exit_code number
---
--- The captured stdout stream of the file.
---@field stdout string
---
--- The captured stderr stream of the file.
---@field stderr string

--- Executes the program referred to by the first parameter
--- and passes any following arguments as cli args.
---
---@param ... string
---@return CmdResult
lake.cmd = function(...)
	local args = List()

	local function recur(x)
		local xtype = type(x)

		if xtype == "string" then
			args:push(x)
		elseif xtype == "table" then
			local xmt = getmetatable(x)

			if TargetGroup == xmt then
				return false, "flattened argument given to lake.cmd resolves to a target group, which is not currently allowed. You must manually resolve this to a list of target paths."
			end

			if Target == xmt then
				args:push(x.path)
			elseif List == xmt then
				for e in x:each() do
					local success, message = recur(e)
					if not success then
						return false, message
					end
				end
			elseif Twine == xmt then
				for s in x:each() do
					args:push(s)
				end
			else
				for _,e in ipairs(x) do
					local success, message = recur(e)
					if not success then
						return false, message
					end
				end
			end
		else
			return false, "flattened argument given to lake.cmd was not resolvable to a string. Its value is "..tostring(x)
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

-- * ---------------------------------------------------------------------------------------------- lake.replace

--- Performs a string replacement on 'subject', which may be a string
--- or a table of strings, a List of strings, or a Twine. When a container is
--- is given a new List of the resulting strings is returned.
--- 
--- This is really just a wrapper over lua's gsub so that we can support
--- containers. See [this page](https://www.lua.org/manual/5.1/manual.html#pdf-string.gsub) 
--- for information on how to use lua's patterns.
---
---@param subject string | string[] | Twine | List<string>
---@param search string
---@param repl string
---@return List<string> | string
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

-- * ---------------------------------------------------------------------------------------------- lake.getMonotonicClock

--- Returns a timestamp in microseconds from the system's monotonic clock.
---
---@return number
lake.getMonotonicClock = function()
	return assert(tonumber(C.lua__getMonotonicClock()))
end

-- * ---------------------------------------------------------------------------------------------- lake.getRealtimeClock

--- Returns a timestamp in microseconds from the system's realtime clock.
---
---@return number
lake.getRealtimeClock = function()
	assert(false, "implement realtime clock at some point :)")
	return 0
end


-- * << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << - << -
--      
--      Target api
--
-- * >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> -


--- A file that the user wants built. Targets may depend on other Targets and 
--- may define a recipe that is used to build them when they don't exist, or 
--- when any of their prerequisites change.
---
--- A target is created or referenced by lake.target(<name>).
---@class Target
---
--- The path used to create the target. This is literally the string passed 
--- to lake.target.
---@field path string
---
--- A handle to lake's internal representation of this Target.
---@field handle userdata?
Target = {}
Target.__index = Target

-- * ---------------------------------------------------------------------------------------------- Target.new

--- Creates a new target at 'path'.
---
---@param path string
---@return Target
Target.new = function(path)
	local o = {}
	setmetatable(o, Target)
	o.path = path
	o.handle = C.lua__createSingleTarget(make_str(path))
	return o
end

-- * ---------------------------------------------------------------------------------------------- Target.__tostring

Target.__tostring = function(self)
	return self.path
end

-- * ---------------------------------------------------------------------------------------------- Target:depends_on

--- Defines the given target as a prerequisite of this one.
---
--- The target this one depends on
---@param x string | Target | List
---@return self
Target.dependsOn = function(self, x)
	local x_type = type(x)
	if "string" == x_type then
		C.lua__makeDep(self.handle, lake.target(x).handle)
	elseif "table" == x_type then
		for v,i in lake.flatten(x):eachWithIndex() do
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

-- * ---------------------------------------------------------------------------------------------- Target:recipe

--- Defines the recipe used to create this target in the event that it does not exist or if any 
--- of its prerequisites are newer than it.
---
--- NOTE that this saves the cwd at the time the recipe is saved and the recipe is executed in 
--- that directory!
---
--- Unlike make, a recipe does not have any implicit variables defined for it to know what its 
--- inputs and outputs are. It is assumed that the function has all the information it needs 
--- captured where it is defined.
---
---@param f function
---@return self
Target.recipe = function(self, f)
	if "function" ~= type(f) then
		error("expected a lua function as the recipe of target '"..self.path.."', got: "..type(f), 2)
	end
	local recipeidx = C.lua__targetSetRecipe(self.handle)
	recipe_table[recipeidx] = co.create(f)
	return self
end

-- * ---------------------------------------------------------------------------------------------- lake.target

--- Either creates the target at 'path' or returns it if it has already been created. Referring to
--- the same target by two different lexical paths is currently undefined behavior! Eg. it is not 
--- advisable to write something like: 
---	```lua
---   lake.target("./src/target.cpp")
---   lake.target("src/target.cpp")
--- ```
--- and expect it to refer to the same file. This may be better defined in the future but for now...
---
---@param path string
---@return Target
lake.target = function(path)
	assert(type(path) == "string", "lake.target passed a non-string value")
	local target = targets[path]
	if target then
		return target
	end
	targets[path] = Target.new(path)
	return targets[path]
end

-- A grouping of targets that are built from a single invocation of a common recipe.
TargetGroup =
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
TargetGroup.dependsOn = function(self, x)
	local x_type = type(x)
	if "string" == x_type then
		C.lua__makeDep(self.handle, lake.target(x).handle)
	elseif "table" == x_type then
		for i,v in ipairs(lake.flatten(x)) do
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
	local args = lake.flatten{...}

	local group = TargetGroup.new()

	for arg,i in args:eachWithIndex() do
		if type(arg) ~= "string" then
			error("flattened argument "..i.." given to lake.targets was not a string, rather a "..type(arg).." with value "..tostring(arg),2)
		end

		local target = lake.target(arg)

		if C.lua__targetAlreadyInGroup(target.handle) ~= 0 then
			error("target '"..target.path.."' is already in a group!",2)
		end

		C.lua__addTargetToGroup(group.handle, target.handle)

		group.targets:insert(target)
	end

	return group
end

lake.__internal = {}
lake.__internal.targets = targets
lake.__internal.recipe_table = recipe_table
lake.__internal.coresume = co.resume
lake.__internal.errhandler = errhandler

return lake
