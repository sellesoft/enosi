#include "lake.h"
#include "platform.h"

#include "stdlib.h"
#include "assert.h"

#include "lexer.h"
#include "parser.h"

#include "target.h"

#include "luahelpers.h"

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#if 0
#define trace(...) printv(__VA_ARGS__)
#else
#define trace(...)
#endif

Lake lake; // global for now, maybe not later 

/* ------------------------------------------------------------------------------------------------ Lake::init
 */
void Lake::init(str p, s32 argc_, const char* argv_[])
{
	path = p;
	parser = (Parser*)mem.allocate(sizeof(Parser));
	parser->init(this);
	argc = argc_;
	argv = argv_;

	// TODO(sushi) get from platform and make adjustable by cli
	max_jobs = 8;
 
	target_pool    = Pool<Target>::create();
	build_queue    = TargetList::create();
	product_list   = TargetList::create();
	active_recipes = TargetList::create();
}

void Lake::print_build_queue()
{
	print("build_queue:\n");

	for (Target& t : build_queue) 
	{
		printv("  ", t.path, "\n"); 
	}
}

void Lake::print_product_list()
{
	print("product_list:\n");

	for (Target& t : product_list)
	{
		printv("  ", t.path, "\n");
	} 
}

void printout(TargetSet& set, u32 depth)
{

	for (Target& t : set)
	{
		for (u32 i = 0; i < depth; i++)
			print(" ");
		printv(t.path, "\n");

		printout(t.prerequisites, depth + 1);
	}
}

/* ------------------------------------------------------------------------------------------------ Lake::run
 */
void Lake::run()
{
	L = lua_open();
	luaL_openlibs(L);
	
	// load the lake module 
	// TODO(sushi) this needs to be baked into the executable
	//             try using string.dump()
	if (luaL_loadfile(L, "src/lake.lua") || lua_pcall(L, 0, 3, 0))
	{
		printf("%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		return;
	}

	// set the targets global
	lua_setglobal(L, lake_targets_table);

	// set the internal lake global
	lua_setglobal(L, lake_internal_table); 

	// set the lake global 
	lua_setglobal(L, "lake");

	parser->start();

	// TODO(sushi) we can just load the program into lua by token 
	//             using the loader callback thing in lua_load
	dstr prog = parser->fin();

	FILE* f = fopen("temp/lakefile", "w");
	fwrite(prog.s, prog.len, 1, f);
	fclose(f);

	if (luaL_loadbuffer(L, (char*)prog.s, prog.len, "lakefile") || lua_pcall(L, 0, 0, 0))
	{
		printf("%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		return;
	}

	prog.destroy();

	for (;;)
	{
		u32 available_workers = max_jobs - active_recipe_count;

		// trace("There are ", available_workers, " available workers.\n");

		for (u32 i = 0; i < available_workers; i++)
		{
			if (build_queue.is_empty())
				break;

			Target* target = build_queue.front();

			trace("Checking target '", target->path, "' from build queue.\n"); 

			if (target->needs_built())
			{
				trace("  Target needs built, adding to active recipes list\n");
				target->active_recipe_node = active_recipes.push_head(target);
				active_recipe_count += 1;
				build_queue.remove(target->build_node);
			}
			else
			{
				trace("  Target does not need built, removing from build queue and decrementing all dependents unsatified prereq counts.\n");
				build_queue.remove(target->build_node);
				for (auto& dependent : target->dependents)
				{
					if (dependent.unsatified_prereq_count == 1)
					{
						trace("  Dependent '", dependent.path, "' has no more unsatisfied prerequisites, adding it to the build queue.\n");
						dependent.build_node = build_queue.push_head(&dependent);
						dependent.unsatified_prereq_count = 0;
					}
					else
						dependent.unsatified_prereq_count -= 1;
				}
			}
		}

		// if no active recipes were added then we must be done
		if (!active_recipes.head)
			break;

		for (auto& t : active_recipes)
		{
			trace("resuming recipe of target ", t.path, "\n");
			switch (t.resume_recipe(L))
			{
				case Target::RecipeResult::Finished: {
					trace("  recipe finished");
					active_recipes.remove(t.active_recipe_node);

					// here we assume the target was built by the recipe
					// and mark its dependents as having a prerequisite that
					// was just built.
					// we could probably have an option later that does 
					// stricter checking.
					for (auto& dependent : t.dependents)
					{
						dependent.flags.set(Target::Flags::PrerequisiteJustBuilt);
						if (dependent.unsatified_prereq_count == 1)
						{
							trace("  Dependent '", dependent.path, "' has no more unsatisfied prerequisites, adding it to the build queue.\n");
							dependent.build_node = build_queue.push_head(&dependent);
							dependent.unsatified_prereq_count = 0;
						}
						else
							dependent.unsatified_prereq_count -= 1;
					}
					active_recipe_count -= 1;
				} break;

				case Target::RecipeResult::Error: {
					error_nopath("recipe error\n");
					exit(1);
				} break;

				case Target::RecipeResult::InProgress: {

				} break;
			}
		}
	}

	lua_getglobal(L, lake_internal_table);
	lua_pop(L, 2);
}

/* ================================================================================================ Lua API
 *  Implementation of the api used in the lua lake module.
 */
extern "C"
{

/* ------------------------------------------------------------------------------------------------ lua__create_target
 */
Target* lua__create_target(str path)
{
	Target* t = lake.target_pool.add();
	*t = Target::create(path);
	t->build_node = lake.build_queue.push_head(t);
	t->product_node = lake.product_list.push_head(t);
	return t;
}

/* ------------------------------------------------------------------------------------------------ lua__make_dep
 */
void lua__make_dep(Target* target, Target* prereq)
{
	trace("Making '", prereq->path, "' a prerequisite of target '", target->path, "\n");
	target->prerequisites.insert(prereq);
	prereq->dependents.insert(target);
	target->unsatified_prereq_count += 1;
	if (prereq->product_node)
	{
		trace("  Prereq is in product list, so it will be removed\n");
		lake.product_list.destroy(prereq->product_node);
		prereq->product_node = nullptr;
	}
	if (target->build_node)
	{
		trace("  Target is in build queue, so it will be removed\n");
		lake.build_queue.destroy(target->build_node);
		target->build_node = nullptr;
	}
}

/* ------------------------------------------------------------------------------------------------ lua__get_cliargs
 */
CLIargs lua__get_cliargs()
{
	return {lake.argc, lake.argv};
}

}
