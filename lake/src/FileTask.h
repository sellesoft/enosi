/*
 *  A Task for a file existance on disk as well as it being newer than any of 
 *  its prerequisites. This is equivalent to make's targets.
 *
 *  The name of these Tasks is an absolute path to the file in question.
 */

#ifndef _lake_filetask_h
#define _lake_filetask_h

#include "Task.h"

/* ================================================================================================ DListIterator  
 */
struct FileTask : public Task
{
  b8 init(String path);

  virtual b8 needRunRecipe() override;
};

#endif 
