#ifndef _lake_lake_h
#define _lake_lake_h

#include "iro/common.h"
#include "iro/containers/list.h"
#include "iro/containers/avl.h"
#include "iro/logger.h"
#include "iro/luastate.h"
#include "iro/process.h"

#include "Task.h"

struct Lexer;
struct Parser;

static const char* lake_targets_table = "__lake__targets";
static const char* lake_recipe_table = "__lake__recipe_table";
static const char* lake_coroutine_resume = "__lake__coroutine_resume";
static const char* lake_err_handler = "__lake__err_handler";

/* ============================================================================
 */
struct ActiveProcess
{
  Process process;
  fs::File stream;
};

/* ============================================================================
 */
struct Lake
{
  LuaState lua;

  TaskList tasks;

  TaskList explicit_requests;

  TaskList leaves;

  TaskList active_recipes;
  u32 active_recipe_count;

  Pool<ActiveProcess> active_process_pool;

  String initpath = nil;

  u32 max_jobs; // --max-jobs <n> or -j <n>
  b8 max_jobs_set_on_cli;

  b8 print_transformed; // --print-transformed

  b8 print_timers = false; // --print-timers

  b8 in_recipe = false;

  b8   init(const char** argv, int argc, mem::Allocator* allocator = &mem::stl_allocator);
  void deinit();

  b8 processArgv(const char** argv, int argc, String* initfile);
  b8 run();

  void addLeaf(Task* task);

  // Central allocation function in case we ever want to change how tasks 
  // are stored.
  template<typename T>
  Task* allocateTask()
  {
    auto* task = (Task*)mem::stl_allocator.allocateType<T>();
    new (task) T;
    return task;
  }

  // Indexes on lua's stack where important things have been loaded.
  // This doesn't have to be tracked at runtime, but makes it easier 
  // to deal with while developing. Ideally once lake is more stable 
  // we can move these to macros with stack indexes we know at compile
  // time.
  struct 
  {
    u16 lake;
    u16 cliargs;
    u16 tasks_by_name;
    u16 coresume;
    u16 errh;
    u16 list;
  } I;
};

#endif
