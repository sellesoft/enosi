#include "lake.h"

#include "stdlib.h"
#include "assert.h"
#include "ctype.h"

#include "target.h"

#include "iro/logger.h"

#include "assert.h"

#include "string.h"

#include "iro/time/time.h"

#include "iro/process.h"
#include "iro/fs/glob.h"
#include "iro/fs/path.h"

#include "iro/platform.h"

#undef stdout
#undef stderr
#undef stdin

using namespace iro;

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

int lua__importFile(lua_State* L);
int lua__cwd(lua_State* L);
int lua__canonicalizePath(lua_State* L);
}

static Logger logger = Logger::create("lake"_str, 

#if LAKE_DEBUG
    Logger::Verbosity::Debug);
#else
    Logger::Verbosity::Notice);
#endif

Lake lake; // global for now, maybe not later 

struct ActiveProcess
{
  Process process;
  fs::File stream;
};

Pool<ActiveProcess> active_process_pool = {};

platform::TermSettings saved_term_settings;

/* ----------------------------------------------------------------------------
 */
b8 Lake::init(const char** argv_, int argc_, mem::Allocator* allocator)
{
  assert(argv_ && allocator);

  INFO("Initializing Lake.\n");
  SCOPED_INDENT;

  max_jobs = 1;

  argc = argc_;
  argv = argv_;

  lua_cli_args = LuaCLIArgList::create(allocator);
  active_process_pool = Pool<ActiveProcess>::create(allocator);
  cwd_stack = Array<fs::Path>::create(allocator);

  // TODO(sushi) also search for lakefile with no extension
  initpath = nil;
  if (!processArgv(&initpath))
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

  lua_cli_arg_iterator = lua_cli_args.head;

  DEBUG("Creating target pool and target lists.\n");

  target_pool    = TargetPool::create(allocator);
  targets        = TargetList::create(allocator);
  build_queue    = TargetList::create(allocator);
  active_recipes = TargetList::create(allocator);
  action_pool    = Pool<str>::create(allocator);
  action_queue   = DList<str>::create(allocator);

  DEBUG("Loading lua state.\n");

  if (!lua.init())
  {
    ERROR("failed to load lua state\n");
    return false;
  }

#define addGlobalCFunc(name) \
  lua.pushcfunction(name); \
  lua.setglobal(STRINGIZE(name));

  addGlobalCFunc(lua__importFile);
  addGlobalCFunc(lua__cwd);
  addGlobalCFunc(lua__canonicalizePath);

#undef addGlobalCFunc

  saved_term_settings = platform::termSetNonCanonical();

  return true;
}

/* ----------------------------------------------------------------------------
 */
void Lake::deinit()
{
  lua.deinit();
  active_recipes.destroy();
  build_queue.destroy();

  for (auto& t : targets)
    t.deinit();
  targets.destroy();
  target_pool.deinit();

  cwd_stack.destroy();
  active_process_pool.deinit();
  lua_cli_args.destroy();
  for (auto& action : action_queue)
    mem::stl_allocator.free(action.bytes);
  action_pool.deinit();
  action_queue.destroy();

  // TODO(sushi) this needs to be done using some platform on exit thing.
  platform::termRestoreSettings(saved_term_settings);
}

struct ArgIter
{
  const char** argv = nullptr;
  u64 argc = 0;
  u64 idx = 0;

  str current = nil;

  ArgIter(const char** argv, u32 argc) : argv(argv), argc(argc) 
  {  
    idx = 1;
    next();
  }

  void next()
  {
    if (idx == argc)
    {
      current = nil;
      return;
    }

    current = { (u8*)argv[idx++] };
    current.len = strlen((char*)current.bytes);
  }
};

/* ----------------------------------------------------------------------------
 */
b8 processMaxJobsArg(Lake* lake, ArgIter* iter)
{
  str mjarg = iter->current;

  iter->next();
  if (isnil(iter->current))
  {
    FATAL("expected a number after '--max-jobs'\n");
    return false;
  }

  str arg = iter->current;

  u8* scan = arg.bytes;

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

  lake->max_jobs = atoi((char*)arg.bytes);

  DEBUG("max_jobs set to ", lake->max_jobs, " via command line argument\n");

  lake->max_jobs_set_on_cli = true;

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 processArgDoubleDash(Lake* lake, str arg, str* initfile, ArgIter* iter)
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
b8 processArgSingleDash(Lake* lake, str arg, str* initfile, ArgIter* iter)
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
b8 Lake::processArgv(str* initfile)
{
  INFO("Processing cli args.\n");
  SCOPED_INDENT;

  for (ArgIter iter(argv, argc); notnil(iter.current); iter.next())
  {
    str arg = iter.current;
    if (arg.len > 1 && arg.bytes[0] == '-')
    {
      if (arg.len > 2 && arg.bytes[1] == '-')
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
      // TODO(sushi) we could just build a list in lua directly here.
      lua_cli_args.pushTail(argv + iter.idx - 1);
      DEBUG("Encountered unknown cli arg '", arg, 
            "' so it has been added to the lua cli args list.\n");
    }
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
static void makeDot(Lake* lake, io::IO* out)
{
  out->write("digraph G \n{\n"_str);

  for (auto& target : lake->targets)
  {
    for (auto& prereq : target.prerequisites)
    {
      io::formatv(out, 
          '"', fs::Path::basename(target.name()), '"', 
          " -> ", 
          '"', fs::Path::basename(prereq.name()), '"', "\n");
    }
  }

  out->write("}"_str);
}

/* ----------------------------------------------------------------------------
 */
b8 Lake::run()
{
  INFO("Loading lake lua module.\n");

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

  INFO("Setting lua globals.\n");

  // TODO(sushi) this can probably be removed in favor of just tracking 
  //             where on the stack these are when returned by the lake module.
  lua.getfield(1, "__internal");

#define setGlobal(name, var) \
    lua.getfield(2, name); \
    lua.setglobal(var);

  setGlobal("targets", lake_targets_table);
  setGlobal("recipe_table", lake_recipe_table);
  setGlobal("coresume", lake_coroutine_resume);
  setGlobal("errhandler", lake_err_handler);

#undef setGlobal

  lua.pop();

  lua.getglobal(lake_err_handler);
  if (!lua.loadfile((char*)initpath.bytes) || 
      !lua.pcall(0,1,2))
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

  // for (auto& target : targets)
  // {
  //   ERROR(target, "\n");
  // }

  if (0) // TODO(sushi) maybe make into a cli arg not very useful tho
  {
    using namespace fs;
    auto dot = 
      File::from("out"_str, 
          OpenFlag::Truncate 
        | OpenFlag::Create
        | OpenFlag::Write);

    if (notnil(dot))
    {
      makeDot(this, &dot);
      dot.close();
    }
  }

  INFO("Beginning build loop.\n");

  // as of rn i think the only code that will run from here on is recipe 
  // callbacks but i could be wrong idk
  lake.in_recipe = true;

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

  return true;
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
Target* lua__createSingleTarget(str path)
{
  Target* t = lake.target_pool.add();
  t->initSingle(path);
  t->build_node = lake.build_queue.pushHead(t);
  lake.targets.pushHead(t);
  INFO("Created target '", t->name(), "'.\n");
  return t;
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
void lua__makeDep(Target* target, Target* prereq)
{
  INFO("Making '", prereq->name(), "' a prerequisite of target '", 
       target->name(), "'.\n");
  SCOPED_INDENT;

  if (!target->prerequisites.has(prereq))
  {
    target->prerequisites.insert(prereq);
    target->unsatified_prereq_count += 1;
  }

  prereq->dependents.insert(target);

  if (target->build_node)
  {
    TRACE("Target is in build queue, so it will be removed\n");
    lake.build_queue.destroy(target->build_node);
    target->build_node = nullptr;
  }
}

/* ----------------------------------------------------------------------------
 *  Sets the target as having a recipe and returns an index into the recipe 
 *  table that the calling function is expected to use to set the recipe 
 *  appropriately.
 */
EXPORT_DYNAMIC
s32 lua__targetSetRecipe(Target* target)
{
  LuaState& lua = lake.lua;

  lua.getglobal(lake_recipe_table);
  defer { lua.pop(); };

  if (target->flags.test(Target::Flags::HasRecipe))
  {
    WARN("Target '", target->name(), "'s recipe is being set again.\n");
    target->recipe_working_directory.destroy();
    target->recipe_working_directory = fs::Path::cwd();
    return target->lua_ref;
  } 
  else
  {
    TRACE("Target '", target->name(), "' is being marked as having a " 
          "recipe.\n");
    target->flags.set(Target::Flags::HasRecipe);
    // TODO(sushi) targets that have their recipe set from the same directory 
    //             should use the same Path?? Probably better to store a set 
    //             (AVL) of cwd Paths and just have the thing on Target be a 
    //             str.
    target->recipe_working_directory = fs::Path::cwd();

    // get the next slot to fill and then set 'next' to whatever that slot 
    // points to
    lua.pushstring("next"_str);
    lua.gettable(-2);

    s32 next = lua.tonumber();
    lua.pop();

    lua.pushstring("next"_str);
    lua.rawgeti(-2, next);

    if (lua.isnil())
    {
      // if we werent pointing at another slot 
      // just set next to the next value
      lua.pop();
      lua.pushinteger(next+1);
    }
    lua.settable(-3);

    target->lua_ref = next;
 
    return next;
  }
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
Target* lua__createGroupTarget()
{
  Target* group = lake.target_pool.add();
  group->initGroup();
  group->build_node = lake.build_queue.pushHead(group);
  lake.targets.pushHead(group);
  INFO("Created group '", (void*)group, "'.\n");
  return group;
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
void lua__addTargetToGroup(Target* group, Target* target)
{
  assertGroup(group);
  assertSingle(target);

  INFO("Adding target '", target->name(), "' to group '", 
       group->name(), "'.\n");
  SCOPED_INDENT;

  group->group.targets.insert(target);
  target->single.group = group;


  if (target->build_node)
  {
    TRACE("Target '", target->name(), "' is in the build queue so it will be "
          "removed.\n");
    lake.build_queue.remove(target->build_node);
    target->build_node = nullptr;
  }
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
b8 lua__targetAlreadyInGroup(Target* target)
{
  assertSingle(target);
  return target->single.group != nullptr;
}


/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
str lua__getTargetPath(Target* target)
{
  assertSingle(target);
  return target->name();
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
const char* lua__nextCliarg()
{
  if (!lake.lua_cli_arg_iterator)
    return nullptr;

  const char* out = *lake.lua_cli_arg_iterator->data;
  lake.lua_cli_arg_iterator = lake.lua_cli_arg_iterator->next;
  return out;
}

/* ----------------------------------------------------------------------------
 *  Finalizes the given group. We assume no more targets will be added to it 
 *  and hash together the hashes of each target in the group to form the hash 
 *  of the group. 
 *
 *  TODO(sushi) implement this. I don't know if it is actually useful, its only usecase I can
 *              think of is referring to the same group by calling 
 *              'lake.targets' with the same paths, but I don't intend on ever 
 *              doing that myself.
 */
EXPORT_DYNAMIC
void lua__finalizeGroup(Target* target)
{
  assertGroup(target);
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
void lua__stackDump()
{
  lake.lua.stackDump();
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
b8 lua__makeDir(str path, b8 make_parents)
{
  return fs::Dir::make(path, make_parents);
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
void lua__dirDestroy(fs::Dir* dir)
{
  mem::stl_allocator.deconstruct(dir);
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
ActiveProcess* lua__processSpawn(str* args, u32 args_count)
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
  } 

  ActiveProcess* proc = active_process_pool.add();
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
  if (proc->process.terminated)
  {
    *out_exit_code = proc->process.exit_code;
    return true;
  }
  return false;
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
void lua__processClose(ActiveProcess* proc)
{
  assert(proc);
  TRACE("closing proc ", (void*)proc, "\n");

  proc->stream.close();
  active_process_pool.remove(proc);
}

/* ----------------------------------------------------------------------------
 *  This could PROBABLY be implemented better but whaaatever.
 */
struct LuaGlobResult
{
  str* paths;
  s32 paths_count;
};

EXPORT_DYNAMIC
LuaGlobResult lua__globCreate(str pattern)
{
  auto matches = Array<str>::create();
  auto glob = fs::Globber::create(pattern);
  glob.run(
    [&matches](fs::Path& p)
    {
      str match = p.buffer.asStr();
      *matches.push() = match.allocateCopy();
      return true;
    });
  glob.destroy();
  return {matches.arr, matches.len()};
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
void lua__globDestroy(LuaGlobResult x)
{
  auto arr = Array<str>::fromOpaquePointer(x.paths);

  for (str& s : arr)
    mem::stl_allocator.free(s.bytes);

  arr.destroy();
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
void lua__setMaxJobs(s32 n)
{
  if (!lake.max_jobs_set_on_cli)
  {
    NOTICE("max_jobs set to ", n, " from lakefile call to lake.maxjobs\n");

    // TODO(sushi) make a thing to get number of logical processors and also 
    //             warn here if max jobs is set too high
    lake.max_jobs = n;
  }
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
s32 lua__getMaxJobs()
{
  return lake.max_jobs;
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
int lua__importFile(lua_State* L)
{
  // TODO(sushi) update to LuaState maybe eventually.
  //             its possible now since LuaState doesnt store anything but the 
  //             lua_State* but im not sure if it will ever store anything else
  //             and I also don't feel like rewriting this atm.
  using Path = fs::Path;

  size_t len;
  const char* s = lua_tolstring(L, -1, &len);
  str path = {(u8*)s, len};

  auto cwd = Path::cwd();
  defer { cwd.chdir(); cwd.destroy(); };

  lua_getglobal(L, lake_err_handler);
  defer { lua_pop(L, 1); };

  if (luaL_loadfile(L, s)) 
  {
    ERROR(lua_tostring(L, -1), "\n");
    lua_pop(L, 1);
    return 0;
  }

  // so dumb 
  auto null_terminated = Path::from(Path::removeBasename(path));
  defer { null_terminated.destroy(); };

  if (!Path::chdir(null_terminated.buffer.asStr()))
    return 0;

  // call the imported file
  int top = lua_gettop(L) - 1;

  if (lua_pcall(L, 0, LUA_MULTRET, 2))
  {
    ERROR(lua_tostring(L, -1), "\n");
    lua_pop(L, 1);
    return 0;
  }

  // check how many thing the file returned
  int nresults = lua_gettop(L) - top;

  // return all the stuff the file returned
  return nresults;
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
int lua__cwd(lua_State* L)
{
  auto cwd = fs::Path::cwd();
  defer { cwd.destroy(); };

  auto s = cwd.buffer.asStr();

  lua_pushlstring(L, (char*)s.bytes, s.len);

  return 1;
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
b8 lua__chdir(str path)
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
  str path = {(u8*)s, len};

  auto canonicalized = fs::Path::from(path);
  defer { canonicalized.destroy(); };

  if (!canonicalized.makeAbsolute())
  {
    ERROR("failed to make path '", path, "' canonical\n");
    return 0;
  }


  lua_pushlstring(L, (char*)canonicalized.buffer.buffer, canonicalized.buffer.len);

  return 1;
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
b8 lua__copyFile(str dst, str src)
{
  return fs::File::copy(dst, src);
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
b8 lua__rm(str path, b8 recursive, b8 force)
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

      str s = str::from(dirent.asSlice());
      if (s == "."_str || s == ".."_str)
        continue;

      Path* full = pathpool.pushHead()->data;
      full->init(direntry->path->buffer.asStr());
      full->makeDir().append(s);

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
b8 lua__pathExists(str path)
{
  return platform::fileExists(path);
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
b8 lua__inRecipe()
{
  return lake.in_recipe;
}

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
b8 lua__touch(str path)
{
  return platform::touchFile(path);
}

}
