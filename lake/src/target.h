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

struct TargetGroup;

/* ================================================================================================ Target
 */
struct Target
{
	enum class Kind
	{
		Single,
		Group,
	};

	Kind kind;

	union
	{
		struct
		{
			str path;
			Target* group;
		} single;

		struct 
		{
			TargetSet targets;
		} group;
	};

	u64 hash;
	
	// index of this target in the lua targets table
	s32 lua_ref;

	// marks used when topologically sorting the graph
	// TODO(sushi) these are not used currently as we do not sort the graph 
	//             which also means if a lakefile specifies an loop we wont 
	//             handle it gracefully!
	b8 perm_mark, temp_mark;

	// this target's node in the target queue
	// null if the target is not in the queue
	TargetListNode* build_node;

	TargetListNode* product_node; // this may no longer be useful

	TargetListNode* active_recipe_node; // this needs a better name

	// list of targets this one depends on
	TargetSet prerequisites;

	// list of targets that depend on this one
	TargetSet dependents;

	// A count of how many prerequisites of this target we have not 
	// 'satisfied' yet. This is decremented as prerequisites of this
	// file are built and once there are none left we know that this 
	// Target is ready to be built.
	u64 unsatified_prereq_count = 0;

	enum class Flags
	{
		// A prereq was built during this lake build so we know immeidately
		// that this target needs to be rebuilt.
		PrerequisiteJustBuilt,

		// The lakefile specified a recipe for this target.
		HasRecipe, 
	};

	typedef ::Flags<Flags> TargetFlags;

	TargetFlags flags;


	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	 */


	// Initialization that is common to all kinds of targets.
	void common_init();

	void init_single(str path);
	void init_group();

	// Returns whether or not this target exists on disk.
	b8 exists();

	// Returns the last modified time of this target as reported by the filesystem.
	// This always queries the target on disk (and so fails if it doesn't exist!).
	// We should try caching the modtime later on, but this may be complicated
	// by needing to understand what may change the modtime during a lake build.
	s64 modtime();
	
	// Returns true if this target is newer than the one given.
	b8 is_newer_than(Target* t);

	// Checks a couple of conditions for building this target:
	//   1. If the target exists at all.
	//   2. If the target has been marked by a prerequisite thats just been built.
	//   3. If the target is older than any of its prerequisites.
	b8 needs_built();

	str name();

	// Returns true if a recipe was added to this target in the lakefile.
	b8 has_recipe();

	enum class RecipeResult
	{
		Error,
		InProgress,
		Finished,
	};

	RecipeResult resume_recipe(lua_State* L);
};

#endif
