/*kel
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

#include "iro/common.h"
#include "iro/unicode.h"
#include "iro/fs/fs.h"
#include "iro/fs/path.h"
#include "iro/containers/list.h"
#include "iro/containers/avl.h"
#include "iro/flags.h"

using namespace iro;

struct Target;

namespace iro{
struct LuaState;
}

/* ------------------------------------------------------------------------------------------------
 *  Hash function for targets.
 *
 *  This will just return the hash cached on the target, which is just a hash of its path string.
 */
u64 hashTarget(const Target* t);

/* ------------------------------------------------------------------------------------------------
 *  Typedefs for qol.
 */
typedef Pool<Target>            TargetPool;
typedef DList<Target>           TargetList;
typedef TargetList::Node        TargetListNode;
typedef DListIterator<Target>   TargetListIterator;
typedef AVL<Target, hashTarget> TargetSet;

struct TargetGroup;

/* ================================================================================================ Target
 */
struct Target
{
  enum class Kind
  {
    Unknown,
    Single,
    Group,
  };

  Kind kind = Kind::Unknown;

  union
  {
    struct
    {
      fs::Path path;
      Target* group;
    } single;

    struct 
    {
      TargetSet targets;
    } group;
  };

  Target() {}

  u64 hash = 0;
  
  // index of this target in the lua targets table
  s32 lua_ref = -1;
  fs::Path recipe_working_directory = nil; // the working directory to execute the recipe from
  // TODO(sushi) move to flags
  b8 recipe_started = false; // set if the target's recipe has been resumed at least once

  // marks used when topologically sorting the graph
  // TODO(sushi) these are not used currently as we do not sort the graph 
  //             which also means if a lakefile specifies an loop we wont 
  //             handle it gracefully!
  // TODO(sushi) when we reimplement top sorting move these to flags.
  b8 perm_mark = false, temp_mark = false;

  // this target's node in the target queue
  // null if the target is not in the queue
  TargetListNode* build_node = nullptr;

  // TODO(sushi) remove this, its only used to remove when polling active
  //             recipes but we already have the node there so this is 
  //             redundant.
  TargetListNode* active_recipe_node = nullptr; // this needs a better name

  // list of targets this one depends on
  TargetSet prerequisites = nil;

  // list of targets that depend on this one
  TargetSet dependents = nil;

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

  TargetFlags flags = {};


  /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
   */


  // Initialization that is common to all kinds of targets.
  void initCommon();

  void initSingle(String path);
  void initGroup();

  void deinit();

  // Returns whether or not this target exists on disk.
  b8 exists();

  // Returns the last modified time of this target as reported by the filesystem.
  // This always queries the target on disk (and so fails if it doesn't exist!).
  // We should try caching the modtime later on, but this may be complicated
  // by needing to understand what may change the modtime during a lake build.
  s64 modtime();
  
  // Returns true if this target is newer than the one given.
  b8 isNewerThan(Target* t);

  // Checks a couple of conditions for building this target:
  //   1. If the target exists at all.
  //   2. If the target has been marked by a prerequisite thats just been built.
  //   3. If the target is older than any of its prerequisites.
  b8 needsBuilt();

  String name();

  // Returns true if a recipe was added to this target in the lakefile.
  b8 hasRecipe();

  enum class RecipeResult
  {
    Error,
    InProgress,
    Finished,
  };

  RecipeResult resumeRecipe(LuaState& lua);

  // when this target is satisfied, handle updating its dependents 
  // and adding them to the build queue
  void updateDependents(TargetList& build_queue, b8 mark_just_built);
};

namespace iro::io
{
s64 format(io::IO* io, Target& t);
}

#endif
