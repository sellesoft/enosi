#ifndef _lake_Lake_h
#define _lake_Lake_h

#include "iro/Common.h"
#include "iro/containers/List.h"
#include "iro/containers/AVL.h"
#include "iro/Logger.h"
#include "iro/LuaState.h"
#include "iro/Process.h"

#include "Task.h"

struct Lexer;
struct Parser;

/* ============================================================================
 */
struct Lake
{
  LuaState lua;

  TaskList tasks;

  TaskList explicit_requests;

  TaskList leaves;

  // If we are currently giving control to some callback of a task, eg. 
  // its condition or recipe function, this will point to it. Important 
  // for cases like a recipe changing its current directory as we need to
  // track this only when they actually change directories. 
  Task* active_task;
 
  TaskList active_recipes;
  u32 active_recipe_count;

  Pool<Process> active_process_pool;

  String initpath = nil;

  u32 max_jobs; // --max-jobs <n> or -j <n>
  b8 max_jobs_set_on_cli;

  b8 print_transformed; // --print-transformed

  b8 print_timers = false; // --print-timers

  b8 in_recipe = false;

  // Handle to the root directory for quickly swapping back when a recipe 
  // needs to chdir into its own dir.
  fs::Dir root_dir;

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
    u16 recipe_err_val;
  } I;
};

#endif
