/*
 *  Representation of a target, which is a file on disk that we would like to
 *  exist and be newer than its dependencies. All targets have some recipe,
 *  a lua function, used to build them.
 *
 *  Currently I am assuming that targets will never be deleted and that 
 *  targets will never have a reason to be removed from another target's 
 *  target set. The only thing a target should ever be removed from for
 *  now is the queue for which we store its node in a doubly linked list. 
 */

#ifndef _lake_target_h
#define _lake_target_h

#include "common.h"
#include "list.h"
#include "avl.h"
#include "flags.h"

struct Target;
struct lua_State;

/* ------------------------------------------------------------------------------------------------
 *  Hash function for targets.
 *
 *  This will just return the hash cached on the target, which is just a hash of its path string.
 */
u64 target_hash(const Target* t);

/* ------------------------------------------------------------------------------------------------
 *  Typedefs for qol.
 */
typedef DList<Target>            TargetList;
typedef TargetList::Node         TargetListNode;
typedef DListIterator<Target>    TargetListIterator;
typedef AVL<Target, target_hash> TargetSet;

/* ================================================================================================ Target
 */
struct Target
{
	str path;

	// a hash of this target's path used for unique identification
	u64 hash;
	
	// marks used when topologically sorting the graph
	b8 perm_mark, temp_mark;

	// this target's node in the target queue
	// null if the target is not in the queue
	TargetListNode* build_node;

	TargetListNode* product_node;

	TargetListNode* active_recipe_node;

	// list of targets this one depends on
	TargetSet prerequisites;

	// list of targets that depend on this one
	TargetSet dependents;

	u64 unsatified_prereq_count;

	typedef u16 FlagType;

	enum class Flags
	{
		// A prereq was built during this lake build so we know immeidately
		// that this target needs to be rebuilt.
		PrerequisiteJustBuilt = 1 << 0,
	};

	typedef ::Flags<Flags> TargetFlags;

	TargetFlags flags;


	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	 */


	static Target create(str path);

	// Returns whether or not this target exists on disk.
	b8 exists();

	// Returns the last modified time of this target as reported by the filesystem.
	// This always queries the target on disk (and so fails if it doesn't exist!).
	// We should try caching the modtime later on, but this may be complicated
	// by needing to understand what may change the modtime during a lake build.
	s64 modtime();
	
	// Returns true if this target is newer than the one given.
	b8 is_newer_than(Target t);

	// Checks a couple of conditions for building this target:
	//   1. If the target exists at all.
	//   2. If the target has been marked by a prerequisite thats just been built.
	//   3. If the target is older than any of its prerequisites.
	b8 needs_built();

	// Returns true if the given lua state has this target in its target map
	// and if that target has a recipe associated with it.
	b8 has_recipe(lua_State* L);

	enum class RecipeResult
	{
		Error,
		InProgress,
		Finished,
	};

	RecipeResult resume_recipe(lua_State* L);

	// Removes this target, removing it from all of its prerequisites 
	// dependents lists and from all of its dependents prerequisites lists.
	void remove();
};

#endif
