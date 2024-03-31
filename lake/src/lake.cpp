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

#define trace(...) printv(__VA_ARGS__)

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

	target_pool  = Pool<Target>::create();
	build_queue  = TargetList::create();
	product_list = TargetList::create();
}

///* ------------------------------------------------------------------------------------------------ visit
// */
//void visit(TargetGraph::Vertex* v, TargetList* sorted)
//{
//	auto t = v->data;
//
//	if (t->perm_mark)
//		return;
//
//	if (t->temp_mark)
//	{
//		error_nopath("cycle detected");
//		exit(1);
//	}
//
//	t->temp_mark = true;
//
//	for (TargetVertexList::Node* iter = v->neighbors.head; iter; iter = iter->next)
//		visit(iter->data, sorted);
//
//	t->temp_mark = false;
//	t->perm_mark = true;
//
//	sorted->push_head(t);
//}
//
///* ------------------------------------------------------------------------------------------------ visit
// */
//void topsort(TargetGraph& g)
//{
//	auto sorted = TargetList::create();
//
//	for (TargetVertexList::Node* iter = g.vertexes.head; iter; iter = iter->next)
//		visit(iter->data, &sorted);
//
//	for (TargetList::Node* n = sorted.head; n; n = n->next)
//		printv(n->data->path, "\n");
//}

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

	print_build_queue();
	print_product_list();

	for (Target& t : build_queue)
	{
		printv("Dependents of ", t.path, ":\n");

		for (Target& dependent : t.dependents)
		{
			printv("  ", dependent.path, "\n");
		}
	}

	for (;;)
	{
		u32 available_workers = max_jobs - active_recipe_count;

		for (u32 i = 0; i < available_workers; i++)
		{
			for (auto& target : build_queue)
			{
				if (target.needs_built())
					active_recipes.push_head(&target);
				else
				{
					build_queue.remove(target.build_node);
					target.remove();
				}
			}
		}

		for (auto& t : active_recipes)
		{
			switch (t.resume_recipe(L))
			{
				case Target::RecipeResult::Finished: {
					active_recipes.remove(t.active_recipe_node);

					// here we assume the target was built by the recipe
					// and mark its dependents as having a prerequisite that
					// was just built.
					// we could probably have an option later that does 
					// stricter checking.
					for (auto& dependent : t.dependents)
						dependent.flags.set(Target::Flags::PrerequisiteJustBuilt);

					// remove the target from the graph
					t.remove();
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
	printv("Making '", prereq->path, "' a prerequisite of target '", target->path, "\n");
	target->prerequisites.insert(prereq);
	prereq->dependents.insert(target);
	target->unsatified_prereq_count += 1;
	if (prereq->product_node)
	{
		printv("  Prereq is in product list, so it will be removed\n");
		lake.product_list.destroy(prereq->product_node);
		prereq->product_node = nullptr;
	}
	if (target->build_node)
	{
		printv("  Target is in build queue, so it will be removed\n");
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
