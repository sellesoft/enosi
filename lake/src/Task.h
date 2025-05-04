/* 
 *  Something to be done if some condition says so, possibly before and/or 
 *  after other tasks. 
 */

#ifndef _lake_Task_h
#define _lake_Task_h

#include "iro/Common.h"
#include "iro/Unicode.h"
#include "iro/containers/List.h"
#include "iro/containers/AVL.h"
#include "iro/fs/Path.h"
#include "iro/fs/Dir.h"

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

  // The working directory of this Task, eg. the directory we will chdir into
  // anytime we hand over control to one of its callbacks, currently only 
  // the cond and recipe callbacks.
  fs::Path wdir;

  // The times at which the recipe started and ended.
  TimePoint start_time = nil;
  TimePoint end_time = nil;

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
    // A lua error occured, such as trying to index a nil value.
    Error,
    // The recipe is currently running.
    InProgress,
    // The recipe yielded with lake.RecipeErr, which indicates the task can not
    // be completed and so we must'nt try to build any dependent tasks.
    // This is distinguished from Error so that we can prevent printing the 
    // lua error like we normally would. 
    // Note that Error also marks this Task has having Errored.
    Failed,
    // The recipe has completed.
    Finished,
  };

  RecipeResult resumeRecipe(Lake& lake);

  enum class Flag
  {
    PrereqJustBuilt,
    HasRecipe,
    HasCond,

    Errored,

    StartedRecipe,
    Complete,

    VisitedPerm,
    VisitedTemp,

    COUNT,
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

