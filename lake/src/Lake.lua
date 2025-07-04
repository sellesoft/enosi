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
local List = require "List"
local Twine = require "Twine"
local Type = require "Type"

--- Global lake table used to report targets and dependencies between them
--- and various helpers.
---
---@class lake
lake = {}

lake.tasks = 
{
  by_name = {},
  by_handle = {},
}

-- * << - << - << - << - << - << - << - << - << - << - << - << - << - << - << -
--      
--      C interface
--
-- * >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> -


local ffi = require "ffi"
ffi.cdef
[[
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

  void* lua__createTask(void* lake, String name);
  void  lua__makeDep(void* task, void* prereq);
  void  lua__setTaskHasRecipe(void* task);
  void  lua__setTaskRecipeWorkingDir(void* task, String wdir);
  u64   lua__getMonotonicClock();
  b8    lua__makeDir(String path, b8 make_parents);
  b8    lua__pathExists(String path);
  b8    lua__inRecipe(void* lake);

  void* lua__processSpawn(void* lake, String* args, u32 args_count);
  void lua__processRead(
    void* proc, 
    void* ptr, u64 len, u64* out_bytes_read);
  b8 lua__processCanRead(void* proc);
  b8 lua__processCheck(void* proc, s32* out_exit_code);
  void lua__processClose(void* lake, void* proc);

  void lua__setMaxJobs(void* lake, s32 n);
  s32  lua__getMaxJobs(void* lake);

  b8 lua__copyFile(String dst, String src);
  b8 lua__moveFile(String dst, String src);
  b8 lua__chdir(String path);
  b8 lua__rm(String path, b8, b8);
  b8 lua__touch(String path);
  u64 lua__modtime(String path);
]]
local C = ffi.C
local strtype = ffi.typeof("String")

---@class str
---@field s cdata
---@field len number

---@param s string
---@return str
local makeStr = function(s)
  if not s then
    print(debug.traceback())
    os.exit(1)
  end
  return strtype(s, #s)
end


-- * << - << - << - << - << - << - << - << - << - << - << - << - << - << - << -
--      
--      Module initialization
--
-- * >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> -


--- A list of command line arguments not consumed internally by lake to be 
--- processed by the user.
lake.cliargs = List{}

local Task = require "Task"


-- * << - << - << - << - << - << - << - << - << - << - << - << - << - << - << -
--      
--      Lake util functions
--
-- * >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> -


-- * --------------------------------------------------------------------------

--- Get a string specifying the current operating system.
---
--- Currently this just returns whatever luajit reports from its os() function
--- (but in lowercase).
--- This could be one of: 
---     windows
---     linux
---     osx
---     bsd
---     posix
---     other
---
-- TODO(sushi) make this return the target operating system if I ever get 
--             around to supporting
--             cross-compilation.
---
---@return string
local os_str = string.lower(jit.os)
lake.os = function()
  return os_str
end

-- * --------------------------------------------------------------------------

--- Get a string specifying the current architecture.
---
--- Currently this just returns whatever luajit reports from its arch() 
--- function.
--- This could be one of:
---   x86
---   x64
---   arm
---   arm64
---   arm64be
---   ppc
---   mips
---   mipsel
---   mips64
---   mips64el
---   mips64r6
---   mips64r6el
---
-- TODO(sushi) make this return the target architecture if I ever get around 
--             to supporting cross-compilation.
---
---@return string
lake.arch = function()
  return jit.arch
end

-- * --------------------------------------------------------------------------

--- Takes a path to a file and returns a canonicalized/absolute
--- path to it. The file must exist!
---
---@param path string
---@return string
lake.canonicalizePath = function(path)
  return lua__canonicalizePath(path)
end

-- * --------------------------------------------------------------------------

--- Touch the file at 'path', updating its modified time to the current time.
--- This does not create a new file.
---
---@param path string
lake.touch = function(path)
  C.lua__touch(makeStr(path))
end

-- * --------------------------------------------------------------------------

--- Takes a table, List or Twine, and flattens all nested tables,
--- Lists, Twines, etc. recursively and returns a new List of 
--- all elements. 
---
--- Note that for tables this only takes array 
--- elements, not key/values.
---
---@param tbl table|List|Twine
---@param handleUnknownType function?
---@return List
lake.flatten = function(tbl, handleUnknownType)
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
        x:each(function(s) out:push(s) end)
      else
        if handleUnknownType then
          local res = handleUnknownType(x, xmt)
          if res then
            out:push(res)
            return
          end
        end

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

-- * --------------------------------------------------------------------------

--- Returns the current working directory as a string
---
---@return string
lake.cwd = function()
  return lua__cwd()
end

-- * --------------------------------------------------------------------------

--- Changes the current working directory to 'path'. Returns
--- a bool determining success.
---
---@param path string
---@return boolean
lake.chdir = function(path)
  if 0 == C.lua__chdir(makeStr(path)) then
    print(debug.traceback())
  end
end

-- * --------------------------------------------------------------------------

--- Attempt to set the max jobs lake will use in building. If this 
--- is set on command line (--max-jobs <n> or -j <n>), this call
--- is ignored.
---
---@param n number
lake.setMaxJobs = function(n)
  C.lua__setMaxJobs(lake.handle, n)
end

-- * --------------------------------------------------------------------------

--- Get the current setting for max jobs.
---
---@return number
lake.getMaxJobs = function()
  return C.lua__getMaxJobs(lake.handle)
end

-- * --------------------------------------------------------------------------

--- Check if the given path exists.
---
---@param path string
---@return boolean
lake.pathExists = function(path)
  return 0 ~= C.lua__pathExists(makeStr(path))
end

-- * --------------------------------------------------------------------------

--- Get the modified time of the file at the given path, or nil if the path 
--- does not exist.
---
---@param path string 
---@return number
lake.modtime = function(path)
  return C.lua__modtime(makeStr(path))
end

-- * --------------------------------------------------------------------------

--- Remove a file or an empty directory.
--- 'options' is an optional table of optional params:
---     * recursive = false
---         Recursively iterate the given path if it is a directory and delete 
---         all files.
---     * force = false
---         Suppress asking the user if each file should be removed.
--- 
--- Returns a bool determining success.
---
---@param path string
---@param options table?
---@return boolean
lake.rm = function(path, options)
  options = options or {recursive = false, force = false}
  return 0 ~=
    C.lua__rm(
      makeStr(path),
      options.recursive or false,
      options.force or false)
end

-- * --------------------------------------------------------------------------

--- Creates the directory at the given path.
--- 'options' is an optional table containing optional parameters
---   * make_parents | bool = false
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

-- * --------------------------------------------------------------------------

--- Copies the file at 'src' to 'dst'.
---
---@param src string
---@param dst string
---@return boolean
lake.copy = function(dst, src)
  return C.lua__copyFile(makeStr(dst), makeStr(src))
end

-- * --------------------------------------------------------------------------

--- Moves the file at 'src' to 'dst'.
---
---@param src string
---@param dst string
---@return boolean
lake.move = function(dst, src)
  return C.lua__moveFile(makeStr(dst), makeStr(src))
end

-- * --------------------------------------------------------------------------

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

  return (List(lua__glob(pattern)))
end

-- * --------------------------------------------------------------------------

lake.getEnvVar = function(name)
  if not name or type(name) ~= "string" then
    error("getEnvVar expects a string")
  end
  return lua__getEnvVar(name)
end

-- * --------------------------------------------------------------------------

lake.setEnvVar = function(name, value)
  if not name or type(name) ~= "string" then
    error("setEnvVar expects a string as its first argument")
  end
  if value and type(value) ~= "string" then
    error("setEnvVar expects a string for its second argument")
  end
  return lua__setEnvVar(name, value)
end

-- * --------------------------------------------------------------------------

--- Executes the program referred to by the first parameter
--- and passes any following arguments as cli args.
---
-- TODO(sushi) add a way to pass callbacks for reading stdout/stderr while the 
--             process is running and maybe even passing a stdin if it ever 
--             seems useful/necessary.
---
---@param args string[] | Twine
---@param options table?
---@return number
lake.cmd = function(args, options)
  -- check if were in a recipe and yield the coroutine 
  -- if so.
  local in_recipe = true

  args = lake.flatten(args, function(x, mt)
    if mt == Target then
      return x.path
    elseif mt == TargetGroup then
      error(
        "flattened cmd argument resolved to a TargetGroup, which cannot be "..
        "stringized!")
    end
  end)

  options = options or {}

  local onRead = options.onRead

  local argsarr = ffi.new("String["..(args:len()+1).."]")

  for arg,i in args:eachWithIndex() do
    if "string" ~= type(arg) then
      local errstr =
        "argument "..tostring(i).." in call to cmd was not a string, it was "..
        "a "..type(arg)..". Arguments were:\n"

      for _,arg in ipairs(args) do
        errstr = " "..errstr..arg.."\n"
      end

      error(errstr)
    end

    if #args[i] ~= 0 then
      argsarr[i-1] = makeStr(args[i])
    end
  end

  local handle = C.lua__processSpawn(lake.handle, argsarr, args:len()+1)

  if not handle then
    local argsstr = ""
    for _,arg in ipairs(args) do
      argsstr = " "..argsstr..arg.."\n"
    end
    error("failed to spawn process with arguments:\n"..argsstr)
  end

  local buffer = require "string.buffer"

  local out_buf = buffer.new()

  local space_wanted = 1024

  local exit_code = ffi.new("s32[1]")
  local out_read = ffi.new("u64[1]")

  local tryRead = function()
    if not onRead then return 0 end

    if 0 == C.lua__processCanRead(handle) then
      return 0
    end

    local ptr, len = out_buf:reserve(space_wanted)

    C.lua__processRead(handle, ptr, len, out_read)

    out_buf:commit(out_read[0])

    if out_read[0] ~= 0 then
      onRead(ffi.string(ptr, out_read[0]))
    end
    return out_read[0]
  end

  while true do
    tryRead()

    local ret = C.lua__processCheck(handle, exit_code)

    if ret ~= 0 then
      -- try to read until we stop recieving data because the process can 
      -- report that its terminated before all of its buffered output is 
      -- consumed
      while tryRead() ~= 0 do end

      C.lua__processClose(lake.handle, handle)

      return assert(tonumber(exit_code[0]))
    else
      co.yield(false)
    end
  end
end

-- * --------------------------------------------------------------------------

--- Returns a timestamp in microseconds from the system's monotonic clock.
---
---@return number
lake.getMonotonicClock = function()
  return assert(tonumber(C.lua__getMonotonicClock()))
end

-- * --------------------------------------------------------------------------

--- Returns a timestamp in microseconds from the system's realtime clock.
---
---@return number
lake.getRealtimeClock = function()
  assert(false, "implement realtime clock at some point :)")
  return 0
end

-- * --------------------------------------------------------------------------

--- Define a callback to be invoked when lake finds that the build has 
--- finished.
---
---@param f function
lake.registerFinalCallback = function(f)
  lake.finalCallback = f
end

-- * << - << - << - << - << - << - << - << - << - << - << - << - << - << - << -
--      
--      Task api
--
-- * >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> -

--- A helper for notifying lake about a Task.
--- The given task must be unique, otherwise an error will occur.
---
---@param task Task
lake.trackTask = function(task)
  if lake.tasks.by_name[task.name] then
    error("a task with name '"..task.name.."' is already tracked")
  end

  lake.tasks.by_name[task.name] = task
end

--- Declares a task that may need to be completed. If a task with the given 
--- name already exists, it will be returned (even if it is some other built
--- in task kind).
---
--- A unique name for this task.
---@param name string
---@return Task
lake.task = function(name)
  local task = lake.tasks.by_name[name]
  if task then
    return task
  end
  return Task.new(lake, name)
end

--- Indicates that an error has occured in a recipe that prevents the task
--- from being completed. Calling this function from within a task's recipe 
--- will mark that task as having errored which will prevent any dependent
--- tasks from attempting to run.
lake.RecipeErr = {}
lake.throwRecipeErr = function()
  co.yield(lake.RecipeErr)
end


-- * << - << - << - << - << - << - << - << - << - << - << - << - << - << - << -
--      
--      Target api
--
-- * >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> - >> -


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

-- * --------------------------------------------------------------------------

--- Creates a new target at 'path'.
---
---@param path string
---@return Target
Target.new = function(path)
  local o = {}
  setmetatable(o, Target)
  o.path = path
  o.handle = C.lua__createSingleTarget(makeStr(path))
  return o
end

-- * --------------------------------------------------------------------------

Target.__tostring = function(self)
  return self.path
end

-- * --------------------------------------------------------------------------

--- Defines the given target as a prerequisite of this one.
---
---@param x string | Target | List The target this one depends on
---@return self
Target.dependsOn = function(self, x)
  local x_type = type(x)
  if "string" == x_type then
    C.lua__makeDep(self.handle, lake.target(x).handle)
  elseif "table" == x_type then
    for v,i in lake.flatten(x):eachWithIndex() do
      local v_type = type(v)
      if "string" ~= v_type then
        error(
          "Element "..i.." of flattened table given to Target.depends_on is "..
          "not a string, rather a "..v_type.." whose value is "..
          tostring(v)..".", 2)
      end
      C.lua__makeDep(self.handle, lake.target(v).handle)
    end
  else
    error(
      "Target.depends_on can take either a string or a table of strings, got: "
      ..x_type..".", 2)
  end
  return self
end

-- * --------------------------------------------------------------------------

--- Defines the recipe used to create this target in the event that it does not 
--- exist or if any of its prerequisites are newer than it.
---
--- NOTE that this saves the cwd at the time the recipe is saved and the recipe 
--- is executed in that directory!
---
--- Unlike make, a recipe does not have any implicit variables defined for it 
--- to know what its inputs and outputs are. It is assumed that the function 
--- has all the information it needs captured where it is defined.
---
---@param f function
---@return self
Target.recipe = function(self, f)
  if "function" ~= type(f) then
    error(
      "expected a lua function as the recipe of target '"..self.path..
      "', got: "..type(f), 2)
  end
  local recipeidx = C.lua__targetSetRecipe(self.handle)
  recipe_table[recipeidx] = co.create(f)
  return self
end

-- * --------------------------------------------------------------------------

--- Either creates the target at 'path' or returns it if it has already been 
--- created. Referring to the same target by two different lexical paths is 
--- currently undefined behavior! Eg. it is not advisable to write something 
--- like: 
--- ```lua
---   lake.target("./src/target.cpp")
---   lake.target("src/target.cpp")
--- ```
--- and expect it to refer to the same file. This may be better defined in 
--- the future but for now...
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

-- A grouping of targets that are built from a single invocation of a common 
-- recipe.
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

return lake
