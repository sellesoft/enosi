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

const char* lake_internal_table = "__lua__internal";

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

	target_pool = Pool<Target>::create();
	target_graph = TargetGraph::create();
	target_queue = TargetList::create();
}

/* ------------------------------------------------------------------------------------------------ visit
 */
void visit(TargetGraph::Vertex* v, TargetList* sorted)
{
	auto t = v->data;

	if (t->perm_mark)
		return;

	if (t->temp_mark)
	{
		error_nopath("cycle detected");
		exit(1);
	}

	t->temp_mark = true;

	for (TargetVertexList::Node* iter = v->neighbors.head; iter; iter = iter->next)
		visit(iter->data, sorted);

	t->temp_mark = false;
	t->perm_mark = true;

	sorted->push_head(t);
}

/* ------------------------------------------------------------------------------------------------ visit
 */
void topsort(TargetGraph& g)
{
	auto sorted = TargetList::create();

	for (TargetVertexList::Node* iter = g.vertexes.head; iter; iter = iter->next)
		visit(iter->data, &sorted);

	for (TargetList::Node* n = sorted.head; n; n = n->next)
		printv(n->data->path, "\n");
}

/* ------------------------------------------------------------------------------------------------ Lake::run
 */
void Lake::run()
{
	lua_State* L = lua_open();
	luaL_openlibs(L);
	
	// load the lake module 
	// TODO(sushi) this needs to be baked into the executable
	//             try using string.dump()
	if (luaL_loadfile(L, "src/lake.lua") || lua_pcall(L, 0, 2, 0))
	{
		printf("%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		return;
	}

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

	topsort(target_graph);

	printv("----------\n");

	for (auto iter = target_graph.vertexes.head; iter; iter = iter->next)
	{
		Target* t = iter->data->data;
		printv(t->path, "\n");

		for (auto neighbor = t->vertex->neighbors.head; neighbor; neighbor = neighbor->next)
		{
			Target* n = neighbor->data->data;
			printv("\t", n->path, "\n");
		}
	}

	printv("-------\n");

	lua_getglobal(L, lake_internal_table);

	// run_recipe is now at the top 

	for (auto iter = target_queue.head; iter; iter = iter->next)
	{
		Target* t = iter->data;
		if (t->exists())
		{
			
		}

		// get the function 
		lua_pushstring(L, "run_recipe");
		lua_gettable(L, -2);
		
		// push target's path as argument
		lua_pushlstring(L, (char*)t->path.s, t->path.len);
		if (lua_pcall(L, 1, 0, 0))
		{
			printf("%s\n", lua_tostring(L, -1));
			lua_pop(L, 1);
			return;
		}
	}

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
	t->vertex = lake.target_graph.add_vertex(t);
	t->queue_node = lake.target_queue.push_head(t);
	return t;
}

/* ------------------------------------------------------------------------------------------------ lua__make_dep
 */
void lua__make_dep(Target* target, Target* dependent)
{
	lake.target_graph.add_edge(target->vertex, dependent->vertex);
	target->dependents.push_head(dependent);
	dependent->prerequisites.push_head(target);
	if (target->queue_node)
	{
		lake.target_queue.remove(target->queue_node);
		target->queue_node = nullptr;
	}
}

/* ------------------------------------------------------------------------------------------------ lua__get_cliargs
 */
CLIargs lua__get_cliargs()
{
	return {lake.argc, lake.argv};
}

}
