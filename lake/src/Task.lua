--- 
--- Definition of Tasks in lake.
---

local List = require "iro.List"
local Type = require "iro.Type"
local ffi = require "ffi"
local C = ffi.C

ffi.cdef
[[
typedef struct Task Task;
typedefstruct Lake Lake;

Task* lua__createLuaTask(Lake* lake, String name);

void lua__setTaskHasCond(Task* task);
void lua__setTaskHasRecipe(Task* task);
]]

local strtype = ffi.typeof("str")
local makeStr = function(s)
  if not s then
    print(debug.traceback())
    os.exit(1)
  end
  return strtype(s, #s)
end

-- * --------------------------------------------------------------------------

--- Something that we might need to do and how to do it. All tasks have a 
--- unique name and a 'recipe' determining how the task is done. 
---
---@class Task : iro.Type
--- A unique name for this Task.
---@field name string
--- A function that is called when this Task needs to be completed. This must
--- exist by the time lake starts its task loop.
---@field recipe function
--- An optional function called when lake wants to know if this Task needs to
--- be completed. Lake assumes this Task is always run if this is nil.
---@field cond function?
--- A handle to lake's internal representation of this Task.
---@field handle userdata
local Task = Type.make()

--- Creates a new Task.
---@param name string
---@return Task | {}
Task.new = function(lake, name)
  local o = {}
  o.name = name
  o.handle = C.lua__createLuaTask(lake.handle, makeStr(name))
  lake.trackTask(o)
  return setmetatable(o, Task)
end

--- Declares the given Task(s) as prerequisites of this Task.
---@param ... Task
---@return self
Task.dependsOn = function(self, ...)
  for task in List{...}:each() do
    if "table" == type(task) then
      if Task:isTypeOf(task) then
        C.lua__makeDep(task.handle, self.handle)
      elseif List:isTypeOf(task) then
        for elem in task:each() do
          self:dependsOn(elem)
        end
      else
        error("unhandled table type passed to dependsOn")
      end
    else
      error("Task.dependsOn expects a typed value")
    end
  end
  return self
end

--- Declares a condition function used to determine if we need to run this 
--- task's recipe.
---@param f function
---@return self
Task.cond = function(self, f)
  if type(f) ~= "function" then
    error("Task.cond only accepts functions, got "..type(f))
  end

  C.lua__setTaskHasCond(self.handle)

  self.cond = f

  return self
end

--- Declares the recipe used to complete this Task.
---@param f function
---@return self
Task.recipe = function(self, f)
  if type(f) ~= "function" then
    error("Task.recipe only accepts functions, got "..type(f))
  end
  
  C.lua__setTaskHasRecipe(self.handle)

  self.recipe = f

  return self
end

return Task
