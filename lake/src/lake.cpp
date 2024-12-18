#include "lake.h"

#include "stdlib.h"
#include "assert.h"
#include "ctype.h"

#include "target.h"
#include "Task.h"

#include "iro/logger.h"

#include "assert.h"

#include "string.h"

#include "iro/time/time.h"

#include "iro/process.h"
#include "iro/fs/glob.h"
#include "iro/fs/path.h"

#include "iro/platform.h"

#include "iro/argiter.h"

using namespace iro;

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

int lua__cwd(lua_State* L);
int lua__canonicalizePath(lua_State* L);
int lua__glob(lua_State* L);
}

#undef stdout
#undef stderr
#undef stdin

static Logger logger = Logger::create("lake"_str, 
    Logger::Verbosity::Notice);

/* ----------------------------------------------------------------------------
 */
b8 Lake::init(const char** argv, int argc, mem::Allocator* allocator)
{
  assert(argv && allocator);

  INFO("initializing Lake.\n");
  SCOPED_INDENT;

  DEBUG("loading lua state.\n");

  if (!lua.init())
  {
    ERROR("failed to load lua state\n");
    return false;
  }

  // TODO(sushi) put these in a table in the lake module or something.
#define addGlobalCFunc(name) \
  lua.pushcfunction(name); \
  lua.setglobal(STRINGIZE(name));

  addGlobalCFunc(lua__cwd);
  addGlobalCFunc(lua__canonicalizePath);
  addGlobalCFunc(lua__glob);

#undef addGlobalCFunc

  INFO("loading lake lua module.\n");

#if LAKE_DEBUG
  if (!lua.loadfile("lake/src/lake.lua") || !lua.pcall(0, 1))
  {
    ERROR("failed to run lake.lua: ", lua.tostring(), "\n");
    lua.pop();
    return false;
  }
#else
  if (!lua.require("lake"_str))
  {
    ERROR("failed to load lake module: ", lua.tostring(), "\n");
    lua.pop();
    return false;
  }
#endif

  I.lake = lua.gettop();

  if (!lua.require("coroutine"_str))
    return ERROR("failed to import coroutine module\n");
  lua.getfield(-1, "resume");
  lua.remove(-2);
  I.coresume = lua.gettop();

  if (!lua.require("errh"_str))
    return ERROR("failed to import error handler\n");
  I.errh = lua.gettop();

  if (!lua.require("list"_str))
    return ERROR("failed to import list\n");
  I.list = lua.gettop();

  lua.getfield(I.lake, "tasks");
  lua.getfield(-1, "by_name");
  lua.remove(-2);
  I.tasks_by_name = lua.gettop();

  lua.pushlightuserdata(this);
  lua.setfield(I.lake, "handle");

  max_jobs = 1;

  // TODO(sushi) also search for lakefile with no extension
  initpath = nil;
  if (!processArgv(argv, argc, &initpath))
    return false;
  b8 resolved = resolve(initpath, "lakefile.lua"_str);

  if (resolved)
    DEBUG("no file specified on cli, searching for 'lakefile.lua'\n");
  else
    DEBUG("file '", initpath, "' was specified as file on cli\n");

  using namespace fs;

  if (!fs::Path::exists(initpath))
  {
    if (resolved)
      FATAL("no file was specified (-f) and no file with the default name "
            "'lakefile.lua' could be found\n");
    else
      FATAL("failed to find specified file (-f) '", initpath, "'\n");
    return false;
  }

  DEBUG("Creating target pool and target lists.\n");

  if (!active_process_pool.init(allocator))
    return ERROR("failed to init active process pool\n");

  active_process_pool = Pool<ActiveProcess>::create(allocator);

  if (!active_recipes.init(allocator)) 
    return false;
  active_recipe_count = 0;

  if (!tasks.init(allocator))
    return ERROR("failed to initialize tasks list\n");

  if (!leaves.init(allocator))
    return ERROR("failed to initialize leaves list\n");

  return true;
}

/* ----------------------------------------------------------------------------
 */
void Lake::deinit()
{
  lua.deinit();
  active_recipes.deinit();

  for (auto* task : tasks.eachPtr())
    task->deinit();

  tasks.deinit();

  active_process_pool.deinit();
}

/* ----------------------------------------------------------------------------
 */
b8 processMaxJobsArg(Lake* lake, ArgIter* iter)
{
  String mjarg = iter->current;

  iter->next();
  if (isnil(iter->current))
  {
    FATAL("expected a number after '--max-jobs'\n");
    return false;
  }

  String arg = iter->current;

  u8* scan = arg.ptr;

  while (*scan)
  {
    if (!isdigit(*scan))
    {
      FATAL("given argument '", arg, "' after '", mjarg, 
            "' must be a number\n");
      return false;
    }

    scan += 1;
  }

  lake->max_jobs = atoi((char*)arg.ptr);

  DEBUG("max_jobs set to ", lake->max_jobs, " via command line argument\n");

  lake->max_jobs_set_on_cli = true;

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 processArgDoubleDash(Lake* lake, String arg, String* initfile, ArgIter* iter)
{
  switch (arg.hash())
  {
  case "verbosity"_hashed:
    {
      iter->next();
      if (isnil(iter->current))
      {
        FATAL("expected a verbosity level after '--verbosity'\n");
        return false;
      }

      switch (iter->current.hash())
      {

#define verbmap(s, level) \
      case GLUE(s, _hashed): \
        DEBUG("Logger verbosity level set to " STRINGIZE(level) \
            " via command line argument.\n"); \
        logger.verbosity = Logger::Verbosity::level;\
        break;
      
      verbmap("trace", Trace);
      verbmap("debug", Debug);
      verbmap("info", Info);
      verbmap("notice", Notice);
      verbmap("warn", Warn);
      verbmap("error", Error);
      verbmap("fatal", Fatal);
      default:
        FATAL("expected a verbosity level after '--verbosity'\n");
        return false;
#undef verbmap

      }
    }
    break;

  case "max-jobs"_hashed:
    if (!processMaxJobsArg(lake, iter))
      return false;
    break;
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 processArgSingleDash(Lake* lake, String arg, String* initfile, ArgIter* iter)
{
  switch (arg.hash())
  {
  case "v"_hashed:
    logger.verbosity = Logger::Verbosity::Debug;
    break;

  case "t"_hashed:
    logger.verbosity = Logger::Verbosity::Trace;
    break;

  case "j"_hashed:
    if (!processMaxJobsArg(lake, iter))
      return false;
    break;
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Lake::processArgv(const char** argv, int argc, String* initfile)
{
  INFO("processing cli args\n");
  SCOPED_INDENT;

  lua.getfield(I.lake, "cliargs");
  I.cliargs = lua.gettop();

  for (ArgIter iter(argv, argc); notnil(iter.current); iter.next())
  {
    String arg = iter.current;
    if (arg.len > 1 && arg.ptr[0] == '-')
    {
      if (arg.len > 2 && arg.ptr[1] == '-')
      {
        if (!processArgDoubleDash(this, arg.sub(2), initfile, &iter))
          return false;
      }
      else
      {
        if (!processArgSingleDash(this, arg.sub(1), initfile, &iter))
          return false;
      }
    }
    else
    {
      lua.pushinteger(1 + lua.objlen(I.cliargs));
      lua.pushstring(iter.current);
      lua.settable(I.cliargs);
    }
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Lake::run()
{
  lua.getglobal(lake_err_handler);
  if (!lua.loadfile((char*)initpath.ptr) || 
      !lua.pcall(0,1))
  {
    ERROR("failed to run ", initpath, ": ", lua.tostring(), "\n");
    lua.pop();
    return false;
  }

  if (lua.isboolean())
  {
    // If the lakefile returns false, dont move onto building.
    // Other other kind of return is ignored.
    // TODO(sushi) make what the lakefile returns explicit, or have them 
    //             signal not to perform building via a lake api function
    //             or something.
    if (!lua.toboolean())
      return true;
    lua.pop();
  }
  lua.pop();

  INFO("beginning build loop.\n");

  TaskList build_queue;
  if (!build_queue.init())
    return ERROR("failed to initialize build queue\n");
  defer { build_queue.deinit(); };

  if (Task::TopSortResult::Cycle == Task::topSortTasks(tasks, &build_queue))
    return ERROR("cycle detected\n");

  for (Task& task : build_queue)
  {
    if (task.isLeaf())
      addLeaf(&task);

  }
    
  for (u64 build_pass = 0, recipe_pass = 0;;)
  {
    if (leaves.isEmpty())
    {
      DEBUG("no leaves, we must be done\n");
      break;
    }

    if (!leaves.isEmpty() && max_jobs > active_recipe_count)
    {
      Task* task = leaves.front();

      DEBUG("checking ", task->name, "\n");

      if (task->needRunRecipe(*this))
      {
        DEBUG("task ", task->name, " needs to run its recipe\n");
        active_recipes.pushHead(task);
        active_recipe_count += 1;
        leaves.remove(leaves.head);
      }
      else
      {
        DEBUG("task ", task->name, " is complete\n");
        task->onComplete(*this, false);
        leaves.remove(leaves.head);
      }
    }

    while (max_jobs <= active_recipe_count)
    {
      TaskList::Node* task_node = active_recipes.head;
      for (;task_node;)
      {
        Task* task = task_node->data;
        TaskList::Node* next = task_node->next;
        switch (task->resumeRecipe(*this))
        {
        case Task::RecipeResult::Finished:
          {
            DEBUG("task ", task->name, " completed\n");
            active_recipes.remove(task_node);
            active_recipe_count -= 1;
            task->onComplete(*this, true);;
          }
          break;

        case Task::RecipeResult::Error:
          {
            task->flags.set(Task::Flag::Errored);
            active_recipes.remove(task_node);
            active_recipe_count -= 1;
          }
          break;

        case Task::RecipeResult::InProgress:
          ;
        }
        task_node = next;
      }
    }
  }

#if 0
  for (u64 build_pass = 0, recipe_pass = 0;;)
  {
    if (!build_queue.isEmpty() && max_jobs - active_recipe_count)
    {
      build_pass += 1;

      if (build_pass % 1000 == 0)
        TRACE("Entering build pass ", build_pass, "\n");
      SCOPED_INDENT;

      while (!build_queue.isEmpty() && max_jobs - active_recipe_count)
      {
        Target* target = build_queue.front();

        INFO("Checking target '", target->name(), "' from build queue.\n"); 
        SCOPED_INDENT;

        if (target->needsBuilt())
        {
          INFO("Target needs built, adding to active recipes list\n");
          target->active_recipe_node = active_recipes.pushHead(target);
          active_recipe_count += 1;
          build_queue.remove(target->build_node);
        }
        else
        {
          INFO("Target does not need built, removing from build queue and "
              "decrementing all dependents unsatified prereq counts.\n");
          SCOPED_INDENT;
          build_queue.remove(target->build_node);
          target->updateDependents(build_queue, false);
        }
      }
    }
    
    if (active_recipes.isEmpty() && build_queue.isEmpty())
    {
      INFO("Active recipe list and build queue are empty, we must be "
           "finished.\n");
      break;
    }

    recipe_pass += 1;

    if (logger.verbosity == Logger::Verbosity::Trace 
        && recipe_pass % 10000 == 0)
    {
      TRACE("Entering recipe pass ", recipe_pass, "\n",
            "Active recipes are: \n");
      SCOPED_INDENT; 

      for (auto& t :active_recipes)
      {
        TRACE(t.name(), "\n");
      }

      TRACE("active count: ", active_recipe_count, "\n"
            "build queue empty? ", build_queue.isEmpty()? "yes" : "no", "\n");
    }

    for (auto& t : active_recipes)
    {
      switch (t.resumeRecipe(lua))
      {
        case Target::RecipeResult::Finished: 
          {
            TRACE("Target '", t.name(), "'s recipe has finished.\n");
            SCOPED_INDENT;

            active_recipes.remove(t.active_recipe_node);
            t.updateDependents(build_queue, true);
            active_recipe_count -= 1;
          } 
          break;

        case Target::RecipeResult::Error:
          ERROR("recipe error\n");
          return false;

        case Target::RecipeResult::InProgress:
          ;
      }

      // Very naive way to try and prevent lake from constantly contesting for 
      // a thread when running a recipe that spawns several processes itself. 
      // Eg. when building llvm the recipe leaves it up to the generated build 
      // system stuff to run multiple jobs. During this lake will be constantly 
      // polling the process for output/termination which wastes an entire 
      // thread of execution that whatever is building llvm could be using.
      //
      // This is a really silly way to handle it and ideally later on should be 
      // adjusted based on how long something is taking. Like, we can time how 
      // long the current recipes have been running for and scale how often we 
      // poll based on that. This would work well to avoid my primary concern 
      // with rate limiting the polling: accurately (sorta) tracking how long 
      // it takes for a recipe to complete. As long as we scale this number to 
      // be negligible compared to the total time the recipe has been running 
      // for, it shouldn't matter. 
      //
      // So,
      // TODO(sushi) setup internal target recipe timers and then fix this 
      //             issue
      platform::sleep(TimeSpan::fromMicroseconds(3));
    }
  }
#endif

  return true;
}

/* ----------------------------------------------------------------------------
 */
void Lake::addLeaf(Task* task)
{
  DEBUG("new leaf: ", task->name, "\n");
  leaves.pushTail(task);
}

#if LAKE_DEBUG

void assertGroup(Target* target)
{
  if (target->kind != Target::Kind::Group)
  {
    FATAL("Target '", (void*)target, "' failed group assertion\n");
    exit(1);
  }
}

void assertSingle(Target* target)
{
  if (target->kind != Target::Kind::Single)
  {
    FATAL("Target '", (void*)target, "' failed single assertion\n");
    exit(1);
  }
}

#else
void assertGroup(Target*){}
void assertSingle(Target*){}
#endif

/* ============================================================================
 *  Implementation of the api used in the lua lake module.
 */
extern "C"
{

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
Task* lua__createTask(Lake* lake, String name)
{
  auto* task = lake->allocateTask<Task>();
  if (!task->init(name))
  {
    ERROR("failed to initialize lua task ", name, "\n");
    mem::stl_allocator.free(task);
    return nullptr;
  }
  lake->tasks.pushTail(task);
  return task;
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
void lua__makeDep(Task* task, Task* prereq)
{
  INFO("Making '", prereq->name, "' a prerequisite of task '", 
       task->name, "'.\n");
  task->prerequisites.insert(prereq);
  prereq->dependents.insert(task);
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
void lua__setTaskHasCond(Task* task)
{
  task->flags.set(Task::Flag::HasCond);
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
void lua__setTaskHasRecipe(Task* task)
{
  task->flags.set(Task::Flag::HasRecipe);
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
void lua__setTaskRecipeWorkingDir(Task* task, String wdir)
{
  if (notnil(task->recipe_wdir))
    task->recipe_wdir.close();
  task->recipe_wdir = fs::Dir::open(wdir);
  if (isnil(task->recipe_wdir))
  {
    ERROR(
      "failed to set working directory of Task '",task->name,"' to ", wdir, 
      "\n");
  }
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
u64 lua__getMonotonicClock()
{
  auto now = TimePoint::monotonic();
  return now.s * 1e6 + now.ns / 1e3;
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
b8 lua__makeDir(String path, b8 make_parents)
{
  return fs::Dir::make(path, make_parents);
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
ActiveProcess* lua__processSpawn(Lake* lake, String* args, u32 args_count)
{
  assert(args && args_count);

  DEBUG("spawning process from file '", args[0], "'\n");
  SCOPED_INDENT;

  if (logger.verbosity <= Logger::Verbosity::Debug)
  {
    DEBUG("with args:\n");
    SCOPED_INDENT;
    for (s32 i = 0; i < args_count; i++)
    {
      DEBUG(args[i], "\n");
    }
    auto cwd = fs::Path::cwd();
    DEBUG("in dir ", cwd);
    cwd.destroy();
  } 

  // TODO(sushi) only use pty when outputting to a terminal or when explicitly
  //             requested via cli args.
  ActiveProcess* proc = lake->active_process_pool.add();
  proc->process = 
    Process::spawnpty(args[0], {args+1, args_count-1}, &proc->stream);
  if (isnil(proc->process))
  {
    ERROR("failed to spawn process using file '", args[0], "'\n");
    exit(1);
  }

  return proc;
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
void lua__processRead(
    ActiveProcess* proc, 
    void* ptr, u64 len, u64* out_bytes_read)
{
  assert(proc && ptr && len && out_bytes_read);
  *out_bytes_read = proc->stream.read({(u8*)ptr, len});
}

/* ----------------------------------------------------------------------------
 *  TODO(sushi) maybe add support for writing someday.
 *
 *  This needs to be explicitly checked now that we use pty's instead of plain
 *  pipes (which is kinda a mistake, I shoulda made that optional!).
 */
EXPORT_DYNAMIC
b8 lua__processCanRead(ActiveProcess* proc)
{
  assert(proc);

  fs::PollEventFlags flags = fs::PollEvent::In;
  if (!proc->stream.poll(&flags))
    return false;

  return flags.test(fs::PollEvent::In);
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
b8 lua__processCheck(ActiveProcess* proc, s32* out_exit_code)
{
  assert(proc and out_exit_code);
  
  proc->process.checkStatus();
  if (proc->process.status == Process::Status::ExitedNormally)
  {
    *out_exit_code = proc->process.exit_code;
    return true;
  }
  else if (proc->process.status == Process::Status::ExitedFatally)
  {
    // Set exit code to one for the process since it crashed.
    *out_exit_code = 1;
    return true;
  }
  return false;
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
void lua__processClose(Lake* lake, ActiveProcess* proc)
{
  assert(proc);
  TRACE("closing proc ", (void*)proc, "\n");

  proc->stream.close();
  proc->process.stop(0);
  lake->active_process_pool.remove(proc);
}

/* ----------------------------------------------------------------------------
 */
int lua__glob(lua_State* L)
{
  auto lua = LuaState::fromExistingState(L);

  String pattern = lua.tostring(1);

  lua.newtable();
  const s32 I_result = lua.gettop();

  auto glob = fs::Globber::create(pattern);
  u64 count = 0;
  glob.run(
    [&lua, &count, I_result](fs::Path& p)
    {
      count += 1;
      lua.pushinteger(count);
      lua.pushstring(p.buffer.asStr());
      lua.settable(I_result);
      return true;
    });
  glob.destroy();
  return 1;
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
void lua__setMaxJobs(Lake* lake, s32 n)
{
  if (!lake->max_jobs_set_on_cli)
  {
    NOTICE("max_jobs set to ", n, " from lakefile call to lake.maxjobs\n");

    // TODO(sushi) make a thing to get number of logical processors and also 
    //             warn here if max jobs is set too high
    lake->max_jobs = n;
  }
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
s32 lua__getMaxJobs(Lake* lake)
{
  return lake->max_jobs;
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
int lua__cwd(lua_State* L)
{
  auto cwd = fs::Path::cwd();
  defer { cwd.destroy(); };

  auto s = cwd.buffer.asStr();

  lua_pushlstring(L, (char*)s.ptr, s.len);

  return 1;
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
b8 lua__chdir(String path)
{
  return fs::Path::chdir(path);
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
int lua__canonicalizePath(lua_State* L)
{
  size_t len;
  const char* s = lua_tolstring(L, 1, &len);
  String path = {(u8*)s, len};

  auto canonicalized = fs::Path::from(path);
  defer { canonicalized.destroy(); };

  if (!canonicalized.makeAbsolute())
  {
    ERROR("failed to make path '", path, "' canonical\n");
    return 0;
  }

  lua_pushlstring(L, (char*)canonicalized.buffer.ptr, canonicalized.buffer.len);

  return 1;
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
b8 lua__copyFile(String dst, String src)
{
  return fs::File::copy(dst, src);
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
b8 lua__rm(String path, b8 recursive, b8 force)
{
  // TODO(sushi) move to iro
  using namespace fs;

  if (!Path::isDirectory(path))
    return File::unlink(path);

  // TODO(sushi) uhh apparently i forgot to implement the non-recursive part 
  //             of this so do that when i actually use it :P
  if (recursive)
  {
    mem::Bump bump;
    bump.init();
    defer { bump.deinit(); };

    u64 file_count = 0;

    struct DirEntry 
    { 
      Dir dir; 
      Path* path; 
      SList<Path> files; 

      DirEntry() : dir(nil), path(nil), files(nil) {} 
      DirEntry(Dir&& dir, Path* path, mem::Bump* bump) 
        : dir(dir), path(path) { files = SList<Path>::create(bump); }
    };

    auto pathpool = DLinkedPool<Path>::create(&bump);
    auto dirpool = DLinkedPool<DirEntry>::create(&bump);
    auto dirstack = DList<DirEntry>::create(&bump);
    auto dirqueue = DList<DirEntry>::create(&bump);

    defer 
    { 
      for (auto& path : pathpool.list)
        path.destroy();

      for (auto& dir : dirpool.list)
        dir.files.deinit();
      // NOTE(sushi) the rest of the mem SHOULD be handled by bump.deinit
    };

    pathpool.pushHead(Path::from(path));
    dirpool.pushHead(DirEntry(Dir::open(path), &pathpool.head(), &bump));
    dirstack.pushHead(&dirpool.head());

    while (!dirstack.isEmpty())
    {
      auto* direntry_node = dirstack.tail;
      DirEntry* direntry = direntry_node->data;

      StackArray<u8, 255> dirent;

      dirent.len = direntry->dir.next({dirent.arr, dirent.capacity()});
      if (dirent.len < 0)
        return false;

      if (dirent.len == 0)
      {
        direntry->dir.close();
        dirstack.remove(direntry_node);
        dirqueue.pushTail(direntry);

        if (dirstack.isEmpty())
          break;

        continue;
      }

      String s = String::from(dirent.asSlice());
      if (s == "."_str || s == ".."_str)
        continue;

      Path* full = pathpool.pushHead()->data;
      full->init(direntry->path->buffer.asStr());
      full->ensureDir().append(s);

      if (full->isDirectory())
      {
        dirpool.pushHead(
            DirEntry(Dir::open(full->buffer.asStr()), full, &bump));
        dirstack.pushTail(&dirpool.head());
      }
      else
      {
        file_count += 1;
        direntry->files.push(full);
      }
    }

    if (!force && file_count)
    {
      io::formatv(&fs::stdout, 
          "Are you sure you want to delete all ", file_count, " files in '", 
          path, "'? [y/N] ");
      u8 c;
      fs::stdin.read({&c, 1});
      io::format(&fs::stdout, "\n");
      if (c != 'y')
        return true;
    }

    for (auto& de : dirqueue)
    {
      for (auto& f : de.files)
      {
        NOTICE("Deleting file: ", f, "\n");
        f.unlink();
      }

      NOTICE("Deleting dir: ", *de.path, "\n");
      de.path->rmdir();
    }
  }
  else
  {
    ERROR("cannot rm path '", path, "' because either its a directory and "
          "recursive was not specified or because I still have not added "
          "non-recursive rm yet ;_;\n");
    return false;
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
b8 lua__pathExists(String path)
{
  return platform::fileExists(path);
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
b8 lua__inRecipe(Lake* lake)
{
  return lake->in_recipe;
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
b8 lua__touch(String path)
{
  return platform::touchFile(path);
}

/* ----------------------------------------------------------------------------
 */
int lua__basicFileTaskCondition(lua_State* L)
{
  auto lua = LuaState::fromExistingState(L);

  String src = lua.tostring(1);
  String dst = lua.tostring(2);
  Task* task = lua.tolightuserdata<Task>(3);

  b8 result = false;

  if (!fs::Path::exists(dst))
  {
    result = true;
  }
  else
  {
  }
}

/* ----------------------------------------------------------------------------
 */
u64 lua__modtime(String path)
{
  TimePoint modtime = fs::Path::modtime(path);
  return ((modtime - TimePoint{}).toMilliseconds());
}

}
