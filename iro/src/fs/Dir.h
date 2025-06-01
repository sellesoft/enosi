/*
 *  Dir utils
 */

#ifndef _iro_Dir_h
#define _iro_Dir_h

#include "../Common.h"
#include "../Unicode.h"

#include "Path.h"
#include "File.h"

namespace iro::fs
{

/* ============================================================================
 *  Type used for streaming directory iteration.
 */
struct Dir
{
  typedef void* Handle;
  Handle handle;

  // Opens a directory stream at 'path'.
  // 'path' must be null-terminated.
  static Dir open(String path);
  static Dir open(Path path);

  // Open a directory stream using a file handle, if that handle represents a 
  // directory.
  static Dir open(File::Handle file_handle);

  // Make a directory at 'path'. If 'make_parents' is true,
  // then all missing parents of the given path will be made as well.
  static b8 make(String path, b8 make_parents = false);
  static b8 make(Path path, b8 make_parents = false);

  // Closes this directory stream.
  void close();

  // Changes the current working directory to this directory.
  b8 chdir();

  // Writes into 'buffer' the name of the next file in the opened directory 
  // and returns the number of bytes written on success. Returns -1 if 
  // we fail for some reason and 0 when we are done.
  s64 next(Bytes buffer);

  DefineNilTrait(Dir, {nullptr}, x.handle == nullptr);
  DefineMoveTrait(Dir, {to.handle = from.handle;});
  DefineScopedTrait(Dir, { x.close(); });
};

}

#endif // _iro_dir_h
