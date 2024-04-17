#include "lake.h"
#include "platform.h"

#include "stdlib.h"
#include "assert.h"
#include "ctype.h"

#include "lexer.h"
#include "parser.h"

#include "target.h"

#include "logger.h"

#include "luahelpers.h"

#include "generated/cliargparser.h"
#include "generated/lakeluacompiled.h"

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

Lake lake; // global for now, maybe not later 
Logger log;

/* ------------------------------------------------------------------------------------------------ Lake::init
 */
b8 Lake::init(str p, s32 argc_, const char* argv_[])
{
#if LAKE_DEBUG
	log.verbosity = Logger::Verbosity::Debug;
#else
	log.verbosity = Logger::Verbosity::Warn;
#endif
	log.indentation = 0;

	INFO("Initializing Lake.\n");
	SCOPED_INDENT;

	path = p;

	max_jobs = 1;

	DEBUG("Allocating and initializing parser.\n");

	parser = (Parser*)mem.allocate(sizeof(Parser));
	parser->init(this);

	argc = argc_;
	argv = argv_;

	lua_cli_args = LuaCLIArgList::create();

	process_argv();

	lua_cli_arg_iterator = lua_cli_args.head;

	DEBUG("Creating target pool and target lists.\n");

	target_pool    = Pool<Target>::create();
	build_queue    = TargetList::create();
	product_list   = TargetList::create();
	active_recipes = TargetList::create();

	DEBUG("Loading lua state.\n");

	L = lua_open();
	luaL_openlibs(L);

	return true;
}

/* ------------------------------------------------------------------------------------------------ Lake::process_argv
 */
b8 Lake::process_argv()
{
	INFO("Processing cli args.\n");
	SCOPED_INDENT;

	for (s32 i = 1; i < argc; i++)
	{
		const char* arg = argv[i];
		CLIArgType type = parse_cli_arg(arg);

		switch (type)
		{
			case CLIArgType::Unknown: {
				if (arg[0] == '-')
				{
					error_nopath("unknown switch '", arg, "', use '--help' for info on how to use lake.");
					exit(1);
				}

				// just hand off the arg to be handled in lake.lua.
				// It is likely a target to be made or a variable set
				lua_cli_args.push(argv + i);

				DEBUG("Encountered unknown cli arg '", arg, "' so it has been added to the lua cli args list.\n");
			} break;

			case CLIArgType::JobsBig:
			case CLIArgType::JobsSmall: {
				// next arg must be a number
				i += 1;

				if (i == argc)
				{
					error_nopath("expected a number after '", arg, "'");
					exit(1);
				}

				arg = argv[i];

				const char* scan = arg;
				while (*scan)
				{
					if (!isdigit(*scan))
					{
						error_nopath("given argument '", arg, "' after '", argv[i-1], "' must be a number");
						exit(1);
					}

					scan += 1;
				}

				max_jobs = atoi(arg);

				DEBUG("max_jobs set to ", max_jobs, " via command line argument.\n");
			} break;

			case CLIArgType::VerboseSmall: {
				log.verbosity = Logger::Verbosity::Debug;
				DEBUG("Logger verbosity level set to Debug via command line argument.\n");
			} break;

			case CLIArgType::VerboseBig: {
				i += 1;

				if (i == argc) 
				{
					error_nopath("expected a verbosity level after '--verbosity'");
					exit(1);
				}

#define SET_VERBOSITY(level) \
				case GLUE(CLIArgType::Option,level): \
					log.verbosity = Logger::Verbosity::level;\
					DEBUG("Logger verbosity level set to " STRINGIZE(level) " via command line argument.\n"); \
					break;

				switch (parse_cli_arg(argv[i]))
				{
					SET_VERBOSITY(Trace);
					SET_VERBOSITY(Debug);
					SET_VERBOSITY(Info);
					SET_VERBOSITY(Notice);
					SET_VERBOSITY(Warn);
					SET_VERBOSITY(Error);
					SET_VERBOSITY(Fatal);
					default: {
						error_nopath("expected a verbosity level after '--verbosity'");
						exit(1);
					} break;
				}

#undef SET_VERBOSITY
			} break;

			case CLIArgType::PrintTransformed: {
				print_transformed = true;

				DEBUG("print_transformed set via command line argument.\n");
			} break;
		}
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ Lake::run
 */
b8 Lake::run()
{

	INFO("Loading lake lua module.\n");

#if LAKE_DEBUG
	if (luaL_loadfile(L, "src/lake.lua") || lua_pcall(L, 0, 5, 0))
	{
		printf("%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		return false;
	}
#else
	if (luaL_loadbuffer(L, (char*)luaJIT_BC_lake, luaJIT_BC_lake_SIZE, "lake.lua") ||
		lua_pcall(L, 0, 5, 0))
	{
		ERROR(lua_tostring(L, -1));
		lua_pop(L, 1);
		return false;
	}
#endif

	INFO("Setting lua globals.\n");

	lua_setglobal(L, lake_coroutine_resume);
	lua_setglobal(L, lake_recipe_table);
	lua_setglobal(L, lake_targets_table);
	lua_setglobal(L, lake_internal_table); 
	lua_setglobal(L, "lake");

	INFO("Starting parser on lakefile.\n");

	parser->start();

	// TODO(sushi) we can just load the program into lua by token 
	//             using the loader callback thing in lua_load
	dstr prog = parser->fin();

	if (print_transformed)
		printf("%.*s\n", prog.len, prog.s);

	INFO("Executing transformed lakefile.\n"); 
	if (luaL_loadbuffer(L, (char*)prog.s, prog.len, "lakefile") || lua_pcall(L, 0, 0, 0))
	{
		printf("%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		return false;
	}

	prog.destroy();

	INFO("Beginning build loop.\n");

	for (u64 build_pass = 0;;)
	{
		if (!build_queue.is_empty() && max_jobs - active_recipe_count)
		{
			build_pass += 1;

			INFO("Entering build pass ", build_pass, "\n");
			SCOPED_INDENT;

			while (!build_queue.is_empty() && max_jobs - active_recipe_count)
			{
				Target* target = build_queue.front();

				INFO("Checking target '", target->name(), "' from build queue.\n"); 
				SCOPED_INDENT;

				if (target->needs_built())
				{
					INFO("Target needs built, adding to active recipes list\n");
					target->active_recipe_node = active_recipes.push_head(target);
					active_recipe_count += 1;
					build_queue.remove(target->build_node);
				}
				else
				{
					INFO("Target does not need built, removing from build queue and decrementing all dependents unsatified prereq counts.\n");
					SCOPED_INDENT;
					build_queue.remove(target->build_node);
					target->update_dependents(build_queue, false);
				}
			}
		}

		
		if (active_recipes.is_empty() && build_queue.is_empty())
		{
			INFO("Active recipe list and build queue are empty, we must be finished.\n");
			break;
		}

		for (auto& t : active_recipes)
		{
			switch (t.resume_recipe(L))
			{
				case Target::RecipeResult::Finished: {
					TRACE("Target '", t.name(), "'s recipe has finished.\n");
					SCOPED_INDENT;

					active_recipes.remove(t.active_recipe_node);
					t.update_dependents(build_queue, true);
					active_recipe_count -= 1;
				} break;

				case Target::RecipeResult::Error: {
					error_nopath("recipe error\n");
					return false;
				} break;

				case Target::RecipeResult::InProgress: {

				} break;
			}
		}
	}

	lua_getglobal(L, lake_internal_table);
	lua_pop(L, 2);

	return true;
}

#if LAKE_DEBUG

void assert_group(Target* target)
{
	if (target->kind != Target::Kind::Group)
	{
		error_nopath("Target '", (void*)target, "' failed group assertion\n");
		exit(1);
	}
}

void assert_single(Target* target)
{
	if (target->kind != Target::Kind::Single)
	{
		error_nopath("Target '", (void*)target, "' failed single assertion\n");
	}
}

#else
void assert_group(Target*){}
void assert_single(Target*){}
#endif

/* ================================================================================================ Lua API
 *  Implementation of the api used in the lua lake module.
 */
extern "C"
{

/* ------------------------------------------------------------------------------------------------ lua__create_single_target
 */
Target* lua__create_single_target(str path)
{
	Target* t = lake.target_pool.add();
	t->init_single(path);
	t->build_node = lake.build_queue.push_head(t);
	t->product_node = lake.product_list.push_head(t);
	INFO("Created target '", t->name(), "'.\n");
	return t;
}

/* ------------------------------------------------------------------------------------------------ lua__make_dep
 */
void lua__make_dep(Target* target, Target* prereq)
{
	INFO("Making '", prereq->name(), "' a prerequisite of target '", target->name(), "'.\n");
	SCOPED_INDENT;

	if (!target->prerequisites.has(prereq))
	{
		target->prerequisites.insert(prereq);
		target->unsatified_prereq_count += 1;
	}

	prereq->dependents.insert(target);

	if (prereq->product_node)
	{
		TRACE("Prereq is in product list, so it will be removed\n");
		lake.product_list.destroy(prereq->product_node);
		prereq->product_node = nullptr;
	}

	if (target->build_node)
	{
		TRACE("Target is in build queue, so it will be removed\n");
		lake.build_queue.destroy(target->build_node);
		target->build_node = nullptr;
	}
}

/* ------------------------------------------------------------------------------------------------ lua__target_set_recipe
 *  Sets the target as having a recipe and returns an index into the recipe table that the calling
 *  function is expected to use to set the recipe appropriately.
 */
s32 lua__target_set_recipe(Target* target)
{
	lua_State* L = lake.L;
	lua_getglobal(L, lake_recipe_table);
	defer { lua_pop(L, 1); };

	if (target->flags.test(Target::Flags::HasRecipe))
	{
		WARN("Target '", target->name(), "'s recipe is being set again.\n");
		return target->lua_ref;
	}	
	else
	{
		TRACE("Target '", target->name(), "' is being marked as having a recipe.\n");
		target->flags.set(Target::Flags::HasRecipe);

		// get the next slot to fill and then set 'next' to whatever that slot points to
		lua_pushlstring(L, "next", 4);
		lua_gettable(L, -2);

		s32 next = lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_pushlstring(L, "next", 4);
		lua_rawgeti(L, -2, next);

		if (lua_isnil(L, -1))
		{
			// if we werent pointing at another slot 
			// just set next to the next value
			lua_pop(L, 1);
			lua_pushnumber(L, next+1);
		}
		lua_settable(L, -3);

		target->lua_ref = next;
 
		return next;
	}

}

/* ------------------------------------------------------------------------------------------------ lua__create_group
 */
Target* lua__create_group_target()
{
	Target* group = lake.target_pool.add();
	group->init_group();
	group->build_node = lake.build_queue.push_head(group);
	INFO("Created group '", (void*)group, "'.\n");
	return group;
}

/* ------------------------------------------------------------------------------------------------ lua__add_target_to_group
 */
void lua__add_target_to_group(Target* group, Target* target)
{
	assert_group(group);
	assert_single(target);

	group->group.targets.insert(target);
	target->single.group = group;
	INFO("Added target '", target->name(), "' to group '", group->name(), "'.\n");

	if (target->build_node)
		lake.build_queue.remove(target->build_node);
}

/* ------------------------------------------------------------------------------------------------ lua__target_already_in_group
 */
b8 lua__target_already_in_group(Target* target)
{
	assert_single(target);
	return target->single.group != nullptr;
}

/* ------------------------------------------------------------------------------------------------ lua__next_cliarg
 */
const char* lua__next_cliarg()
{
	if (!lake.lua_cli_arg_iterator)
		return nullptr;

	const char* out = *lake.lua_cli_arg_iterator->data;
	lake.lua_cli_arg_iterator = lake.lua_cli_arg_iterator->next;
	return out;
}

/* ------------------------------------------------------------------------------------------------ lua__finalize_group
 *  Finalizes the given group. We assume no more targets will be added to it and hash together the
 *  hashes of each target in the group to form the hash of the group. 
 *
 *  TODO(sushi) implement this. I don't know if it is actually useful, its only usecase I can
 *              think of is referring to the same group by calling 'lake.targets' with the 
 *              same paths, but I don't intend on ever doing that myself.
 */
void lua__finalize_group(Target* target)
{
	assert_group(target);
}

/* ------------------------------------------------------------------------------------------------ lua__stack_dump
 */
void lua__stack_dump()
{
	stack_dump(lake.L);
}

}
