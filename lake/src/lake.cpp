#include "lake.h"
#include "platform.h"

#include "stdlib.h"
#include "assert.h"

#include "lexer.h"
#include "parser.h"

#include "target.h"

#include "logger.h"

#include "luahelpers.h"

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

	log.verbosity = Logger::Verbosity::Trace;
	log.indentation = 0;

	INFO("Initializing Lake.\n");
	SCOPED_INDENT;

	path = p;

	DEBUG("Allocating and initializing parser.\n");

	parser = (Parser*)mem.allocate(sizeof(Parser));
	parser->init(this);

	argc = argc_;
	argv = argv_;

	// TODO(sushi) get from platform and make adjustable by cli
	//             this should probably just default to 1
	//             and the cli can manually set it via -j
	//             and the lakefile can manually set it 
	//             or request lake to set it to an amount
	//             appropriate for the number of cores we 
	//             have.
	//             however, the cli setting should always
	//             override whatever the lakefile does.
	max_jobs = 4;
  
	DEBUG("Creating target pool and target lists.\n");

	target_pool       = Pool<Target>::create();
	build_queue       = TargetList::create();
	product_list      = TargetList::create();
	active_recipes    = TargetList::create();
	target_group_pool = Pool<TargetGroup>::create();

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

	for (u32 i = 0; i < argc; i++)
	{
		const char* arg = argv[i];
		const char* scan = arg;

		if (scan[0] == '-')
		{
			scan += 1;

			if (scan[0] == '-')
			{
				ERROR("There are no cli args beginning with '--' yet.\n");
				return false;
			}

			
		}
	}

	return true;
}

/* ------------------------------------------------------------------------------------------------ Lake::run
 */
b8 Lake::run()
{

	INFO("Loading lake lua module.\n");

	// TODO(sushi) this needs to be baked into the executable
	//             try using string.dump()
	if (luaL_loadfile(L, "src/lake.lua") || lua_pcall(L, 0, 3, 0))
	{
		printf("%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		return false;
	}

	INFO("Setting lua globals.\n");

	lua_setglobal(L, lake_targets_table);
	lua_setglobal(L, lake_internal_table); 
	lua_setglobal(L, "lake");

	INFO("Starting parser on lakefile.\n");

	parser->start();

	// TODO(sushi) we can just load the program into lua by token 
	//             using the loader callback thing in lua_load
	dstr prog = parser->fin();

//	FILE* f = fopen("temp/lakefile", "w");
//	fwrite(prog.s, prog.len, 1, f);
//	fclose(f);

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

				INFO("Checking target '", target->path, "' from build queue.\n"); 
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
					for (auto& dependent : target->dependents)
					{
						if (dependent.unsatified_prereq_count == 1)
						{
							INFO("Dependent '", dependent.path, "' has no more unsatisfied prerequisites, adding it to the build queue.\n");
							dependent.build_node = build_queue.push_tail(&dependent);
							dependent.unsatified_prereq_count = 0;
						}
						else
						{
							dependent.unsatified_prereq_count -= 1;
							TRACE("Dependent '", dependent.path, "' has ", dependent.unsatified_prereq_count, (dependent.unsatified_prereq_count == 1? " unsatisfied prereq left.\n" : " unsatisfied prereqs left.\n"));
						}
					}
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
					TRACE("Target '", t.path, "'s recipe has finished.\n");
					SCOPED_INDENT;

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
							INFO("Dependent '", dependent.path, "' has no more unsatisfied prerequisites, adding it to the build queue.\n");
							dependent.build_node = build_queue.push_tail(&dependent);
							dependent.unsatified_prereq_count = 0;
						}
						else
							dependent.unsatified_prereq_count -= 1;
					}
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
	INFO("Created target '", t->path, "'.\n");
	return t;
}

/* ------------------------------------------------------------------------------------------------ lua__make_dep
 */
void lua__make_dep(Target* target, Target* prereq)
{
	INFO("Making '", prereq->path, "' a prerequisite of target '", target->path, "\n");
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

/* ------------------------------------------------------------------------------------------------ lua__target_set_has_recipe
 *  This is primarily to avoid having to look into the lua state to check this.
 */
void lua__target_set_has_recipe(Target* target)
{
	TRACE("Target '", target->path, "' is being marked as having a recipe.\n");
	target->flags.set(Target::Flags::HasRecipe);
}

/* ------------------------------------------------------------------------------------------------ lua__create_group
 */
TargetGroup* lua__create_group()
{
	TargetGroup* group = lake.target_group_pool.add();
	group->targets = TargetSet::create();
	return group;
}

/* ------------------------------------------------------------------------------------------------ lua__add_target_to_group
 */
void lua__add_target_to_group(TargetGroup* group, Target* target)
{
	group->targets.insert(target);
	target->group = group;
}

/* ------------------------------------------------------------------------------------------------ lua__target_already_in_group
 */
b8 lua__target_already_in_group(Target* target)
{
	return target->group != nullptr;
}

/* ------------------------------------------------------------------------------------------------ lua__get_cliargs
 */
CLIargs lua__get_cliargs()
{
	return {lake.argc, lake.argv};
}

}
