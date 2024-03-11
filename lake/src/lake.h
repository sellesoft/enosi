#ifndef _lake_lake_h
#define _lake_lake_h

#include "common.h"
#include "graph.h"

struct Target;

typedef Graph<Target>           TargetGraph;
typedef Graph<Target>::Vertex   TargetVertex;
typedef TargetGraph::VertexList TargetVertexList;
typedef DList<Target>           TargetList;
typedef TargetList::Node        TargetListNode;

struct Lexer;
struct Parser;

struct Lake
{
	str path;

	Parser* parser;

	// graph connecting targets
	TargetGraph target_graph;
	// queue of targets that have no dependencies
	TargetList target_queue;

	Pool<Target> target_pool;

	s32          argc;
	const char** argv;

	void init(str path, s32 argc, const char* argv[]);
	void run();
};

extern Lake lake;

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
