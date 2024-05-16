#ifndef _lake_lake_h
#define _lake_lake_h

#include "iro/common.h"
#include "iro/containers/list.h"
#include "iro/containers/avl.h"
#include "iro/logger.h"

#include "target.h"

struct Lexer;
struct Parser;

struct lua_State;

static const char* lake_internal_table = "__lake__internal";
static const char* lake_targets_table = "__lake__targets";
static const char* lake_recipe_table = "__lake__recipe_table";
static const char* lake_coroutine_resume = "__lake__coroutine_resume";

typedef SList<const char*> LuaCLIArgList;

struct Lake
{
	str path;

	Parser* parser;

	lua_State* L;

	// queue of targets that are ready to be built
	// if necessary, which are those that either
	// dont have prerequisites or have had all of 
	// their prerequisites satified.
	TargetList build_queue;

	// list tracking targets that aren't prerequisites
	// for any other targets, aka the final targets 
	// we want to build.
	TargetList product_list; 

	TargetList active_recipes;
	u32 active_recipe_count;

	Pool<Target> target_pool;

	// cli args that are handled within lake.lua
	LuaCLIArgList lua_cli_args; 
	LuaCLIArgList::Node* lua_cli_arg_iterator;

	Logger logger = Logger::create("lake"_str, Logger::Verbosity::Trace);

	s32          argc;
	const char** argv;

	fs::File initfile = nil;

	u32 max_jobs; // --max-jobs <n> or -j <n>
	b8 print_transformed; // --print-transformed

	b8   init(const char** argv, int argc, mem::Allocator* allocator = &mem::stl_allocator);
	void deinit();

	b8 process_argv(str* initfile);
	b8 run();
};

extern Lake lake;
extern Logger log;

// api used in the lua lake module 
extern "C" 
{
	Target* lua__create_target(str path);
	void    lua__make_dep(Target* target, Target* dependent);

	typedef struct CLIargs
	{
		s32 argc;
		const char** argv;
	} CLIargs; 

	CLIargs lua__get_cliargs();
}

#endif
