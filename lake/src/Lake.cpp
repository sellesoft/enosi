#include "Lake.h"

#include "stdlib.h"
#include "assert.h"
#include "ctype.h"

#include "Task.h"

#include "iro/Logger.h"

#include "assert.h"

#include "string.h"

#include "iro/time/Time.h"

#include "iro/Process.h"
#include "iro/fs/Glob.h"
#include "iro/fs/Path.h"

#include "iro/Platform.h"

#include "iro/ArgIter.h"

using namespace iro;

extern "C"
{
#include "luajit/lua.h"

EXPORT_DYNAMIC
int lua__cwd(lua_State* L);
EXPORT_DYNAMIC
int lua__canonicalizePath(lua_State* L);
EXPORT_DYNAMIC
int lua__glob(lua_State* L);
EXPORT_DYNAMIC
int lua__getEnvVar(lua_State* L);
EXPORT_DYNAMIC
int lua__setEnvVar(lua_State* L);
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
  addGlobalCFunc(lua__getEnvVar);
  addGlobalCFunc(lua__setEnvVar);

#undef addGlobalCFunc

  INFO("loading lake lua module.\n");

  if (!lua.require("Lake"_str))
  {
    ERROR("failed to load lake module: ", lua.tostring(), "\n");
    lua.pop();
    return false;
  }

  I.lake = lua.gettop();

  if (!lua.require("coroutine"_str))
    return ERROR("failed to import coroutine module\n");
  lua.getfield(-1, "resume");
  lua.remove(-2);
  I.coresume = lua.gettop();

  if (!lua.require("Errh"_str))
    return ERROR("failed to import error handler\n");
  I.errh = lua.gettop();

  if (!lua.require("List"_str))
    return ERROR("failed to import list\n");
  I.list = lua.gettop();

  lua.getfield(I.lake, "tasks");
  lua.getfield(-1, "by_name");
  lua.remove(-2);
  I.tasks_by_name = lua.gettop();

  lua.getfield(I.lake, "RecipeErr");
  I.recipe_err_val = lua.gettop();

  lua.pushlightuserdata(this);
  lua.setfield(I.lake, "handle");

  max_jobs = 1;

  // TODO(sushi) also search for lakefile with no extension
  initpath = nil;
  max_jobs_set_on_cli = false;
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

  // TODO(sushi) maybe just use raw process handles and remove iro's
  //             whole Process interface in favor of just putting
  //             Process platform stuff in its own interface and
  //             splitting up the Platform.h stuff into different
  //             files to begin with.
  active_process_pool = Pool<Process>::create(allocator);

  if (!active_recipes.init(allocator))
    return false;
  active_recipe_count = 0;

  if (!tasks.init(allocator))
    return ERROR("failed to initialize tasks list\n");

  if (!leaves.init(allocator))
    return ERROR("failed to initialize leaves list\n");

  root_dir = fs::Dir::open("."_str);
  if (isnil(root_dir))
    return ERROR("failed to get handle to root directory\n");

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
static b8 processMaxJobsArg(Lake* lake, ArgIter* iter)
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

  lake->max_jobs = strtol((char*)arg.ptr, nullptr, 10);

  DEBUG("max_jobs set to ", lake->max_jobs, " via command line argument\n");

  lake->max_jobs_set_on_cli = true;

  return true;
}

/* ----------------------------------------------------------------------------
 */
static b8 processArgDoubleDash(
    Lake* lake,
    String arg,
    ArgIter* iter)
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

  case "print-timers"_hashed:
    lake->print_timers = true;
    break;

  default:;
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
static b8 processArgSingleDash(
    Lake* lake,
    String arg,
    ArgIter* iter)
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

  default:;
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
        if (!processArgDoubleDash(this, arg.sub(2), &iter))
          return false;
      }
      else
      {
        if (!processArgSingleDash(this, arg.sub(1), &iter))
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
  auto lakefile_start = TimePoint::monotonic();

  lua.require("Errh"_str);
  if (!lua.loadfile((char*)initpath.ptr) ||
      !lua.pcall(0,1))
  {
    ERROR("failed to run ", initpath, ": ", lua.tostring(), "\n");
    lua.pop();
    return false;
  }

  if (print_timers)
    NOTICE("lakefile took ",
           WithUnits(TimePoint::monotonic() - lakefile_start), "\n");

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

  // for (Task& task : tasks)
  // {
  //   INFO("task ", task.name, ":\n");
  //
  //   INFO("  prereqs:\n");
  //   for (Task& prereq : task.prerequisites)
  //   {
  //     INFO("    ", prereq.name, "\n");
  //   }
  // }

  TaskList build_queue;
  if (!build_queue.init())
    return ERROR("failed to initialize build queue\n");
  defer { build_queue.deinit(); };

  auto sort_start = TimePoint::monotonic();

  if (Task::TopSortResult::Cycle == Task::topSortTasks(tasks, &build_queue))
    return ERROR("cycle detected\n");

  if (print_timers)
    NOTICE("top sort took ", WithUnits(TimePoint::monotonic() - sort_start),
         "\n");

  for (Task& task : build_queue)
  {
    if (task.isLeaf())
      addLeaf(&task);
  }

  b8 success = true;
  for (u64 build_pass = 0, recipe_pass = 0;;)
  {
    if (leaves.isEmpty() && active_recipes.isEmpty())
    {
      DEBUG("no leaves, we must be done\n");
      break;
    }

    while (!leaves.isEmpty() && max_jobs > active_recipe_count)
    {
      Task* task = leaves.front();

      DEBUG("checking '", task->name, "'\n");

      if (task->needRunRecipe(*this))
      {
        DEBUG("task '", task->name, "' needs to run its recipe\n");
        active_recipes.pushHead(task);
        active_recipe_count += 1;
        leaves.remove(leaves.head);
      }
      else
      {
        DEBUG("task '", task->name, "' is complete\n");
        task->onComplete(*this, false);
        leaves.remove(leaves.head);
      }
    }

    while (max_jobs <= active_recipe_count ||
          (active_recipe_count && leaves.isEmpty()))
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
            DEBUG("task '", task->name, "' completed\n");
            active_recipes.remove(task_node);
            active_recipe_count -= 1;
            task->onComplete(*this, true);;
          }
          break;

        case Task::RecipeResult::Failed:
        case Task::RecipeResult::Error:
          {
            task->flags.set(Task::Flag::Errored);
            active_recipes.remove(task_node);
            active_recipe_count -= 1;
            success = false;
          }
          break;

        case Task::RecipeResult::InProgress:
          ;
        }
        task_node = next;
      }
    }
  }

  lua.pushvalue(I.lake);
  lua.pushstring("finalCallback"_str);
  lua.gettable(-2);
  if (!lua.isnil())
  {
    lua.pushboolean(success);
    if (!lua.pcall(1, 0))
      return ERROR("lake.finalCallback failed: ", lua.tostring(), "\n");
  }
  lua.pop(); // Pop the table.

  return true;
}

/* ----------------------------------------------------------------------------
 */
void Lake::addLeaf(Task* task)
{
  DEBUG("new leaf: ", task->name, "\n");
  leaves.pushTail(task);
}

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
  if (notnil(task->wdir))
    task->wdir.destroy();
  task->wdir = fs::Path::from(wdir);
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
Process* lua__processSpawn(Lake* lake, String* args, u32 args_count)
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
    DEBUG("in dir ", cwd, "\n");
    cwd.destroy();
  }

  Process* proc = lake->active_process_pool.add();
  *proc =
    Process::spawn(args[0], {.ptr=args+1, .len=args_count-1}, nil);
  if (isnil(*proc))
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
    Process* proc,
    void* ptr, u64 len, u64* out_bytes_read)
{
  assert(proc && ptr && len && out_bytes_read);
  *out_bytes_read = proc->read({(u8*)ptr, len});
}

/* ----------------------------------------------------------------------------
 *  TODO(sushi) maybe add support for writing someday.
 */
EXPORT_DYNAMIC
b8 lua__processCanRead(Process* proc)
{
  assert(proc);
  return proc->hasOutput();
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
b8 lua__processCheck(Process* proc, s32* out_exit_code)
{
  assert(proc and out_exit_code);

  proc->check();
  if (proc->status == Process::Status::ExitedNormally)
  {
    *out_exit_code = proc->exit_code;
    return true;
  }

  if (proc->status == Process::Status::ExitedFatally)
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
void lua__processClose(Lake* lake, Process* proc)
{
  assert(proc);
  TRACE("closing proc ", (void*)proc, "\n");

  proc->close();

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
    // NOTICE("max_jobs set to ", n, " from lakefile call to lake.maxjobs\n");

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
b8 lua__moveFile(String dst, String src)
{
  return fs::File::rename(dst, src);
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

      dirent.len = direntry->dir.next({dirent.arr, u64(dirent.capacity())});
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
	if (!platform::unlinkFile(path))
	{
		ERROR("cannot rm path '", path, "'\n");
		return false;
	}
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
EXPORT_DYNAMIC
u64 lua__modtime(String path)
{
  TimePoint modtime = fs::Path::modtime(path);
  return ((modtime - TimePoint{}).toMilliseconds());
}

/* ----------------------------------------------------------------------------
 */
int lua__getEnvVar(lua_State* L)
{
	auto lua = LuaState::fromExistingState(L);
	if (!lua.isstring(1))
		return ERROR("lua__getEnvVar expects a string as first argument\n");

	String var = lua.tostring(1);
	TRACE("lua__getEnvVar(\"", var, "\")\n");

	s32 bytes_needed = platform::getEnvVar(var, nil);
	if (bytes_needed == -1)
	{
	 lua.pushnil();
	 return 1;
	}

  auto buffer = Bytes::from(
      (u8*)mem::stl_allocator.allocate(bytes_needed), bytes_needed);
  if (buffer.ptr == nullptr)
    return ERROR("lua__getEnvVar failed to allocate space for a string"
                 " of ", bytes_needed, " bytes\n");

  s32 bytes_written = platform::getEnvVar(var, buffer);
  DEBUG("lua__getEnvVar(\"", var, "\") == \"", String::from(buffer), "\"\n");

	lua.pushstring(String::from(buffer));

  mem::stl_allocator.free(buffer.ptr);

	return 1;
}

/* ----------------------------------------------------------------------------
 */
int lua__setEnvVar(lua_State* L)
{
	auto lua = LuaState::fromExistingState(L);
	String name = lua.tostring(1);
	String val = lua.tostring(2);

  if (!platform::setEnvVar(name, val))
	  return 0;

  lua.pushboolean(true);
  return 1;
}

}
