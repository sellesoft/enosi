/*
 *  Representation of a target, which is a file on disk that we would like to
 *  exist and be newer than its dependencies. All targets have some recipe,
 *  a lua function, used to build them.
 */

#ifndef _lake_target_h
#define _lake_target_h

#include "common.h"
#include "platform.h"
#include "lake.h"

/* ================================================================================================ Target
 */
struct Target
{
	str path;

	// Ref to the lua function used as this target's recipe
	s32 recipe_ref;
	
	// marks used when topologically sorting the graph
	b8 perm_mark, temp_mark;

	// this target's vertex in the target graph
	TargetVertex* vertex;

	// this target's node in the target queue
	// null if the target is not in the queue
	TargetListNode* queue_node;

	// list of targets this one depends on
	TargetList prerequisites;

	// list of targets that depend on this one
	TargetList dependents;


	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	 */


	/* -------------------------------------------------------------------------------------------- create
	 */
	static Target create(str path)
	{
		Target out = {};
		out.path = path;
		out.n_dependencies = 0;
		out.recipe_ref = 0;
		return out;
	}

	/* -------------------------------------------------------------------------------------------- exists
	 *  Returns whether or not this target exists on disk.
	 */
	b8 exists()
	{
		return ::path_exists(path);
	}

	/* -------------------------------------------------------------------------------------------- modtime
	 */
	s64 modtime()
	{
		// TODO(sushi) what if our path is not null-terminated ?
		return ::modtime((char*)path.s);
	}

	/* -------------------------------------------------------------------------------------------- is_newer_than
	 */
	b8 is_newer_than(Target t)
	{
		return modtime() > t.modtime();
	}
};

#endif
