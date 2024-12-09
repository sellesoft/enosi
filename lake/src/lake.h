#ifndef _lake_lake_h
#define _lake_lake_h

#include "iro/common.h"
#include "iro/containers/list.h"
#include "iro/containers/avl.h"
#include "iro/logger.h"
#include "iro/luastate.h"

#include "target.h"
#include "Task.h"

struct Lexer;
struct Parser;

static const char* lake_targets_table = "__lake__targets";
static const char* lake_recipe_table = "__lake__recipe_table";
static const char* lake_coroutine_resume = "__lake__coroutine_resume";
static const char* lake_err_handler = "__lake__err_handler";

typedef DList<const char*> LuaCLIArgList;

struct Lake
{
  LuaState lua;

  // queue of targets that are ready to be built
  // if necessary, which are those that either
  // dont have prerequisites or have had all of 
  // their prerequisites satified.
  TargetList build_queue;

  TargetList active_recipes;
  u32 active_recipe_count;

  TargetList explicit_requests;

  TargetList targets;

  Pool<Target> target_pool;

  // cli args that are handled within lake.lua
  LuaCLIArgList lua_cli_args; 
  LuaCLIArgList::Node* lua_cli_arg_iterator;

  Pool<String> action_pool;
  DList<String> action_queue;

  TaskList tasks;

  s32          argc;
  const char** argv;

  String initpath = nil;

  u32 max_jobs; // --max-jobs <n> or -j <n>
  b8 max_jobs_set_on_cli;

  b8 print_transformed; // --print-transformed

  b8 in_recipe = false;

  Array<fs::Path> cwd_stack;

  b8   init(const char** argv, int argc, mem::Allocator* allocator = &mem::stl_allocator);
  void deinit();

  b8 processArgv(String* initfile);
  b8 run();

};

extern Lake lake;

// api used in the lua lake module 
extern "C" 
{
  Target* lua__create_target(String path);
  void    lua__make_dep(Target* target, Target* dependent);

  typedef struct CLIargs
  {
    s32 argc;
    const char** argv;
  } CLIargs; 

  CLIargs lua__get_cliargs();
}

#endif
