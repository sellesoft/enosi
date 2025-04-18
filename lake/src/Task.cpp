#include "Task.h"

#include "Lake.h"

#include "iro/Logger.h"

static Logger logger = 
  Logger::create("lake.task"_str, Logger::Verbosity::Trace);

/* ----------------------------------------------------------------------------
 */
u64 hashTask(const Task* task) { return task->uid; }

/* ----------------------------------------------------------------------------
 */
b8 Task::init(String name)
{
  this->name = name.allocateCopy();
  uid = name.hash();

  if (!prerequisites.init())
    return ERROR(
        "failed to initialize prerequisites list for task ", name, "\n");

  if (!dependents.init())
    return ERROR("failed to initialize dependents list for task", name, "\n");

  wdir = fs::Path::cwd();

  return true;
}

/* ----------------------------------------------------------------------------
 */
void Task::deinit()
{
  mem::stl_allocator.free(name.ptr);
  name = {};
  uid = 0;

  prerequisites.deinit();
  dependents.deinit();
}

/* ----------------------------------------------------------------------------
 */
b8 Task::isLeaf()
{
  for (Task& task : prerequisites)
  {
    if (!task.flags.test(Flag::Complete) || task.flags.test(Flag::Errored))
      return false;
  }
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Task::needRunRecipe(Lake& lake)
{
  if (!flags.test(Flag::HasCond))
    return false;
  if (flags.test(Flag::PrereqJustBuilt))
    return true;

  LuaState& lua = lake.lua;

  wdir.chdir();
  defer 
  { 
    // Always save whatever the current directory is as the user may have 
    // chdir'd during this callback.
    // TODO(sushi) I have a bunch of problems with this, especially in 
    //             the recipe resuming case, as it means we are constantly
    //             reallocating a path string and changing directories
    //             even in cases where we will just be chdir'ing right
    //             back into the same dir. This might not be a huge deal
    //             but idk it bothers me :P.
    //             Originally I tried just storing a handle to the dir on 
    //             Linux, but due to the amount of Tasks we may have this 
    //             caused us to hit the file handle limit. There are prob ways
    //             to get around this, but using file handles like this likely
    //             isn't easy to get working on Windows, so whatever.
    wdir.destroy();
    wdir = fs::Path::cwd();
    lake.root_dir.chdir(); 
  };

  s32 top = lua.gettop();
  defer { lua.settop(top); };

  lua.pushstring(name);
  lua.gettable(lake.I.tasks_by_name);
  lua.getfield(-1, "cb");
  lua.getfield(-1, "cond");

  lua.pushvalue(lake.I.list);
  lua.newtable();
  u32 prereq_count = 0;
  for (Task& prereq : prerequisites)
  {
    prereq_count += 1;
    lua.pushinteger(prereq_count);
    lua.pushstring(prereq.name);
    lua.settable(-3);
  }

  if (!lua.pcall(1, 1))
    return ERROR(
      "failed to make prereq list for task cond:\n", lua.tostring(), "\n");

  defer { lua.pop(2); };

  if (!lua.pcall(1, 1))
    return ERROR(
      "failed to run cond for Task ",name,":\n",lua.tostring(),"\n");

  defer { lua.pop(); };

  if (lua.toboolean())
    return true;
  else
    return false;
}

/* ----------------------------------------------------------------------------
 */
void Task::onComplete(Lake& lake, b8 just_built)
{
  flags.set(Flag::Complete);
  for (Task& dependent : dependents)
  {
    // TODO(sushi) this kinda sucks. We used to do this better by 
    //             tracking a count of unsatisfied prereqs on Targets
    //             but for some reason I'm hesitant to do that here.
    //             Probably return to doing that, or even just use
    //             an extrinsic graph structure that we can trim 
    //             nodes from to detect this kinda stuff.
    if (dependent.isLeaf())
      lake.addLeaf(&dependent);
    if (just_built)
      dependent.flags.set(Flag::PrereqJustBuilt);
  }
}

/* ----------------------------------------------------------------------------
 */
static Task::TopSortResult topSortVisit(Task* task, TaskList* sorted)
{
  if (task->flags.test(Task::Flag::VisitedPerm))
    return Task::TopSortResult::Ok;

  if (task->flags.test(Task::Flag::VisitedTemp))
  {
    ERROR(task->name, " <- \n");
    return Task::TopSortResult::Cycle;
  }

  task->flags.set(Task::Flag::VisitedTemp);

  for (Task& prereq : task->prerequisites)
  {
    Task::TopSortResult result = topSortVisit(&prereq, sorted);
    if (result == Task::TopSortResult::Cycle)
    {
      ERROR(task->name, " <- \n");
      return result;
    }
    else if (result != Task::TopSortResult::Ok)
    {
      return result;
    }
  }

  task->flags.set(Task::Flag::VisitedPerm);
  sorted->pushTail(task);

  return Task::TopSortResult::Ok;
}

/* ----------------------------------------------------------------------------
 */
Task::TopSortResult Task::topSortTasks(const TaskList& tasks, TaskList* sorted)
{
  for (Task& task : tasks)
  {
    if (!task.flags.test(Flag::VisitedPerm))
    {
      TopSortResult result = topSortVisit(&task, sorted);
      if (result != TopSortResult::Ok)
        return result;
    }
  }
  return TopSortResult::Ok;
}

/* ----------------------------------------------------------------------------
 */
Task::RecipeResult Task::resumeRecipe(Lake& lake)
{
  if (!flags.test(Flag::HasRecipe))
  {
    ERROR("task '",name,"' has no recipe set!\n");
    return RecipeResult::Error;
  }

  if (isnil(start_time))
    start_time = TimePoint::monotonic();

  if (!wdir.chdir())
  {
    ERROR("failed to chdir into Task's working directory: '",wdir,"'\n");
    return Task::RecipeResult::Error;
  }

  defer
  {
    wdir.destroy();
    wdir = fs::Path::cwd();
    lake.root_dir.chdir();
  };

  LuaState& lua = lake.lua;
    
  lua.pushstring(name);
  lua.gettable(lake.I.tasks_by_name);

  if (lua.isnil())
  {
    ERROR("failed to get lua table for task '",name,"'\n");
    return RecipeResult::Error;
  }

  lua.getfield(-1, "cb");

  defer { lua.pop(2); };

  lua.pushvalue(lake.I.coresume);
  lua.getfield(-2, "recipe");

  if (!lua.pcall(1, 2))
  {
    ERROR(lua.tostring(), "\n");
    return RecipeResult::Error;
  }

  defer { lua.pop(2); };

  // Check for a lua error having occured during the coroutine.
  if (!lua.toboolean(-2))
  {
    ERROR(lua.tostring(), "\n");
    return RecipeResult::Error;
  }

  // Check if the coroutine threw lake.RecipeErr
  if (lua.equal(lake.I.recipe_err_val, -1))
  {
    return RecipeResult::Failed;
  }

  if (lua.isnil())
  {
    end_time = TimePoint::monotonic();
    return RecipeResult::Finished;
  }
  else
    return RecipeResult::InProgress;
}
