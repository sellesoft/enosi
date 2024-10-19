/*
 *  Common filesystem stuff.
 */

#ifndef _iro_fs_h
#define _iro_fs_h

#include "../traits/move.h"

#include "path.h"
#include "file.h"
#include "dir.h"

namespace iro::fs
{

enum class DirWalkResult
{
  Stop,     // completely stop the walk 
  Next,     // move to the next file in the current directory
  StepInto, // step into the current file if it is a directory
  StepOut,  // step out of the current directory
};

template<typename F>
concept DirWalkCallback = requires(F f, MayMove<Path>& path)
{ { f(path) } -> std::convertible_to<DirWalkResult>; };

// Walk the given directory using callback 'f' starting at 'path'.
// The callback controls the walk by returning one of DirWalkResult.
void walk(String path, DirWalkCallback auto f, mem::Allocator* allocator = &mem::stl_allocator);

namespace __internal
{

/* ------------------------------------------------------------------------------------------------
 */
b8 walkForReal(Path& path, DirWalkCallback auto f)
{
  auto dir = Dir::open(path);

  // if were starting in cwd we dont want it to appear in the path we give
  // so just clear it 
  // TODO(sushi) this is kinda dumb
  if (path.isCurrentDirectory())
    path.clear();

  if (isnil(dir))
    return false;

  defer { dir.close(); };

  // Path we offer to the user via the callback, which they can move 
  // elsewhere if they want to take it.
  MayMove<Path> user_path;
  defer { if (!user_path.wasMoved()) user_path->destroy(); };

  for (;;)
  {
    // rollback to the original path after were done messing with this file
    auto rollback = path.makeRollback();
    defer { path.commitRollback(rollback); };

    Bytes buf = path.reserve(255);

    s64 len = dir.next(buf);
    if (len < 0)
      return false;
    else if (len == 0)
      return true;
    
    path.commit(len);

    if (path.isParentDirectory() || path.isCurrentDirectory())
      continue;

    if (user_path.wasMoved()) 
    {
      // if the user took the copy of the path, make another one
      user_path = path.copy();
    }
    else 
    {
      // otherwise just write into the same buffer
      user_path->clear();
      user_path->append(path.buffer.asStr());
    }

    auto result = f(user_path);

    switch (result)
    {
    case DirWalkResult::Stop: 
      return false;
    
    case DirWalkResult::Next:
      break;

    case DirWalkResult::StepInto:
      path.makeDir();
      if (!walkForReal(path, f))
        return false;
      break;

    case DirWalkResult::StepOut:
      return true;
    }
  }
}

}

/* ------------------------------------------------------------------------------------------------
 */
void walk(String pathin, DirWalkCallback auto f, mem::Allocator* allocator)
{
  auto path = scoped(Path::from(pathin, allocator));
  __internal::walkForReal(path, f);
}

}



#endif // _iro_fs_h
