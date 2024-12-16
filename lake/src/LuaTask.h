/*
 *  A plain task created via lua, where the run condition is a lua function.
 */

#ifndef _lake_luatask_h
#define _lake_luatask_h

#include "Task.h"

/* ============================================================================
 */
struct LuaTask : public Task
{
  // NOTE(sushi) the condition function is stored on the lua task table.
  //             If no condition was provided, we just assume this task
  //             is intended to always run.

  virtual b8 needRunRecipe() override;
};

#endif
