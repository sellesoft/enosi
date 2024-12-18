/* 
 *  Something to be done if some condition says so, possibly before and/or 
 *  after other tasks. 
 */

#ifndef _lake_task_h
#define _lake_task_h

#include "iro/common.h"
#include "iro/unicode.h"
#include "iro/containers/list.h"
#include "iro/containers/avl.h"
#include "iro/fs/path.h"
#include "iro/fs/dir.h"

using namespace iro;

struct Lake;
struct Task;

u64 hashTask(const Task* t);

typedef DList<Task> TaskList;
typedef TaskList::Node TaskNode;
typedef AVL<Task, hashTask> TaskSet;

/* ============================================================================
 */
struct Task
{
  // Unique id of this Task, which is a hash of its name. 
  u64 uid = 0;

  // A unique name for this Task. Note that this String is owned by this Task.
  String name = {};

  // Lists of Tasks this Task either depends on or is a prerequisite of.
  // This forms the dependency graph.
  TaskSet prerequisites = {};
  TaskSet dependents = {};

  // The working directory of this Task's recipe. This can be changed as the 
  // recipe is executed!
  fs::Dir recipe_wdir;

  b8   init(String name);
  void deinit();

  // Returns true if all of this Task's prerequisites are marked as completed
  // and none of its prerequisites are marked as Errored.
  b8 isLeaf();

  // Returns if this Task's recipe should run after all of its prerequisites 
  // have been built.
  b8 needRunRecipe(Lake& lake);

  void onComplete(Lake& lake, b8 just_built);

  // Starts or resumes this task's recipe.
  enum class RecipeResult
  {
    Error,
    InProgress,
    Finished,
  };

  RecipeResult resumeRecipe(Lake& lake);

  enum class Flag
  {
    PrereqJustBuilt,
    HasRecipe,
    HasCond,

    Errored,

    RunningRecipe,
    Complete,

    VisitedPerm,
    VisitedTemp,
  };
  typedef Flags<Flag> Flags;

  Flags flags = {};

  enum class TopSortResult
  {
    Ok,
    Cycle,
  };

  static TopSortResult topSortTasks(const TaskList& tasks, TaskList* sorted);
};

#endif

