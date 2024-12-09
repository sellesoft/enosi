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
      String s = {}, 
      mem::Allocator* allocator = &mem::stl_allocator);

  static Path cwd(mem::Allocator* allocator = &mem::stl_allocator);

  // Unlink a file. For directories use 'rmdir'.
  static b8 unlink(String path);

  // Remove a directory.
  static b8 rmdir(String path);

  // Change the current working directory into the one at 'path'.
  static b8 chdir(String path);

  // Checks if the given name matches the given glob-style pattern.
  static b8 matches(String name, String pattern);

  // Checks if something exists at the given path.
  static b8 exists(String path);

  // Checks if the given path is a file or directory.
  static b8 isRegularFile(String path);
  static b8 isDirectory(String path);

  // Retrieve the basename of the given path, eg. the last element in the 
  // path.
  static String basename(String path);

  // Retrieve a String with the basename of the given path removed.
  static String removeBasename(String path);

  // Compares the modification times of the given paths.
  //  0 is returned if they are equal, 
  //  1 is returned if path0 > path1,
  // -1 is returned if path1 < path0.
  // This function asserts if either of the files do not exist.
  static s8 compareModTimes(String path0, String path1);

  // Method api. Very messy..

  b8 init(mem::Allocator* allocator = &mem::stl_allocator) 
  { 
    return init({}, allocator); 
  }

  b8 init(String s, mem::Allocator* allocator = &mem::stl_allocator);

  void destroy();

  Path copy();
  void clear();

  // Equivalent to the static api, but applied to this Path.
  b8 unlink() { return unlink(buffer.asStr()); }
  b8 rmdir() { return rmdir(buffer.asStr()); }

  Path& append(io::Formattable auto... args) 
    { io::formatv(&buffer, args...); return *this; }

  Path& appendDir(String dir) { append("/"_str, dir); return *this; }
  Path& appendDir(Slice<u8> dir) 
    { append("/"_str, String::from(dir)); return *this; }

  Bytes reserve(s32 space) { return buffer.reserve(space); }
  void  commit(s32 space) { buffer.commit(space); }

  b8 makeAbsolute();

  Path& ensureDir() 
  { 
    if (buffer.len != 0 && buffer.ptr[buffer.len-1] != '/') 
      append('/'); 
    return *this; 
  }

  b8 chdir() { return chdir(buffer.asStr()); }

  // Returns the final component of this path eg.
  // src/fs/path.h -> path.h
  String basename() { return basename(buffer.asStr()); }

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
  b8 matches(String pattern);

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
