/*
 *  Path to some File in the filesystem. This is just a lexical representation, the file named may 
 *  or may not exist.
 *
 *  Provides helpers to manipulate and query information about whatever is at the given path.
 *
 *  Paths must use '/' as the separator.
 *  TODO(sushi) normalize paths when I start getting stuff to work on Windows
 *  TODO(sushi) this api is a huuuuge mess as its just been me piling stuff onto it as I need it.
 *              organize it later PLEASE
 */

#ifndef _iro_path_h
#define _iro_path_h

#include "../common.h"
#include "../unicode.h"
#include "../io/io.h"
#include "../io/format.h"
#include "../traits/move.h"
#include "../traits/scoped.h"

namespace iro::fs
{

struct File;

struct Path
{
  // TODO(sushi) replace this with either a manual implementation of a 
  //             dynamic buffer or some more general byte buffer as 
  //             the way this is being used is really pushing what io::Memory
  //             is meant to be used for.
  //             plus, the reading api on IO is useless here
  io::Memory buffer; // dynamic buffer for now, maybe make static later

  // Makes a new Path from the given string.
  static Path from(
      str s = {}, 
      mem::Allocator* allocator = &mem::stl_allocator);

  static Path cwd(mem::Allocator* allocator = &mem::stl_allocator);

  // Unlink a file. For directories use 'rmdir'.
  static b8 unlink(str path);

  // Remove a directory.
  static b8 rmdir(str path);

  // Change the current working directory into the one at 'path'.
  static b8 chdir(str path);
  static b8 matches(str name, str pattern);
  static b8 exists(str path);
  static b8 isRegularFile(str path);
  static b8 isDirectory(str path);

  static str basename(str path);
  static str removeBasename(str path);

  // Compares the modification times of the given paths.
  //  0 is returned if they are equal, 
  //  1 is returned if path0 > path1,
  // -1 is returned if path1 < path0.
  // This function asserts if either of the files do not exist.
  static s8 compareModTimes(str path0, str path1);

  b8   init(mem::Allocator* allocator = &mem::stl_allocator) 
    { return init({}, allocator); }
  b8   init(str s, mem::Allocator* allocator = &mem::stl_allocator);
  void destroy();

  b8 unlink() { return unlink(buffer.asStr()); }
  b8 rmdir() { return rmdir(buffer.asStr()); }

  Path copy();

  void clear();

  Path& appendDir(str dir) { append("/"_str, dir); return *this; }
  Path& appendDir(Slice<u8> dir) 
    { append("/"_str, str::from(dir)); return *this; }
  Path& append(io::Formattable auto... args) 
    { io::formatv(&buffer, args...); return *this; }

  Bytes reserve(s32 space) { return buffer.reserve(space); }
  void  commit(s32 space) { buffer.commit(space); }

  b8 makeAbsolute();

  // TODO(sushi) rename cause this can be confused with actually creating the 
  //             directory at this path
  Path& makeDir() 
  { 
    if (buffer.len != 0 && buffer.ptr[buffer.len-1] != '/') 
      append('/'); 
    return *this; 
  }

  b8 chdir() { return chdir(buffer.asStr()); }

  // Returns the final component of this path eg.
  // src/fs/path.h -> path.h
  str basename() { return basename(buffer.asStr()); }

  void removeBasename() { buffer.len = removeBasename(buffer.asStr()).len; }

  // Helpers for querying information about the file at this path

  b8 exists() { return Path::exists(buffer.asStr()); }

  b8 isRegularFile() { return Path::isRegularFile(buffer.asStr()); }
  b8 isDirectory() { return Path::isDirectory(buffer.asStr()); }

  inline b8 isCurrentDirectory() 
  { 
    return 
      (buffer.len == 1 && 
       buffer[1] == '.') 
      || 
      (buffer.len > 1 && 
       buffer[buffer.len-1] == '.' && 
       buffer[buffer.len-2] == '/'); 
  }

  inline b8 isParentDirectory() 
  { 
    return 
      (buffer.len == 2 && 
       buffer[0] == '.' && 
       buffer[1] == '.') 
      || 
      (buffer.len > 2 && 
       buffer[buffer.len-1] == '.' && 
       buffer[buffer.len-2] == '.' && 
       buffer[buffer.len-3] == '/');
  }

  // Returns if this path matches the given pattern
  // TODO(sushi) write up the specification of patterns that can be used here
  //             its just Linux shell globbing 
  b8 matches(str pattern);

  typedef io::Memory::Rollback Rollback;

  Rollback makeRollback() { return buffer.createRollback(); }
  void commitRollback(Rollback rollback) { buffer.commitRollback(rollback); }
};

}

namespace iro::io
{
  
static s64 format(io::IO* io, fs::Path& x)
{
  return io::format(io, x.buffer.asStr());
}

}

DefineMove(iro::fs::Path, { to.buffer = move(from.buffer); });
DefineNilValue(iro::fs::Path, {}, { return isnil(x.buffer); });
DefineScoped(iro::fs::Path, { x.destroy(); });

#endif // _iro_path_h
