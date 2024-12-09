#include "Task.h"

#include "iro/logger.h"

static Logger logger = 
  Logger::create("lake.task"_str, Logger::Verbosity::Notice);

/* ----------------------------------------------------------------------------
 */
u64 hashTask(const Task* task) { return task->uid; }

/* ----------------------------------------------------------------------------
 */
b8 Task::init(String name)
{
  this->name = name.allocateCopy();
  uid = name.hash();

  if (!prerequisites.init())
    return ERROR(
        "failed to initialize prerequisites list for task ", name, "\n");

  if (!dependents.init())
    return ERROR("failed to initialize dependents list for task", name, "\n");

  working_dir = fs::Path::cwd();

  return true;
}

/* ----------------------------------------------------------------------------
 */
void Task::deinit()
{
  mem::stl_allocator.free(name.ptr);
  name = {};
  uid = 0;

  prerequisites.deinit();
  dependents.deinit();

  working_dir.destroy();
}

/* ----------------------------------------------------------------------------
 */
static Task::TopSortResult topSortVisit(Task* task, TaskList* sorted)
{
  if (task->flags.test(Task::Flag::VisitedPerm))
    return Task::TopSortResult::Ok;

  if (task->flags.test(Task::Flag::VisitedTemp))
    return Task::TopSortResult::Cycle;

  task->flags.set(Task::Flag::VisitedTemp);

  for (Task& prereq : task->prerequisites)
  {
    Task::TopSortResult result = topSortVisit(&prereq, sorted);
    if (result != Task::TopSortResult::Ok)
      return result;
  }

  task->flags.set(Task::Flag::VisitedPerm);
  sorted->pushHead(task);

  return Task::TopSortResult::Ok;
}

/* ----------------------------------------------------------------------------
 */
Task::TopSortResult Task::topSortTasks(const TaskList& tasks, TaskList* sorted)
{
  for (Task& task : tasks)
  {
    if (!task.flags.test(Flag::VisitedPerm))
    {
      TopSortResult result = topSortVisit(&task, sorted);
      if (result != TopSortResult::Ok)
        return result;
    }
  }
  return TopSortResult::Ok;
}
