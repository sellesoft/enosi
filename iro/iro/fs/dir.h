/*
 *  Dir utils
 */

#ifndef _iro_dir_h
#define _iro_dir_h

#include "../common.h"
#include "../unicode.h"

#include "path.h"
#include "file.h"

namespace iro::fs
{

/* ================================================================================================ fs::Dir
 *  Type used for streaming directory iteration.
 */
struct Dir
{
	typedef void* Handle;
	Handle handle;

	// Opens a directory stream at 'path'.
	// 'path' must be null-terminated.
	static Dir open(str path);
	static Dir open(Path path);

	// Open a directory stream using a file handle, if that handle represents a 
	// directory.
	static Dir open(File::Handle file_handle);

	// Make a directory at 'path'. If 'make_parents' is true,
	// then all missing parents of the given path will be made as well.
	static b8 make(str path, b8 make_parents = false);
	static b8 make(Path path, b8 make_parents = false);

	// Closes this directory stream.
	void close();

	// Writes into 'buffer' the name of the next file in the opened directory 
	// and returns the number of bytes written on success. Returns -1 if 
	// we fail for some reason and 0 when we are done.
	s64 next(Bytes buffer);
};

}

DefineMove(iro::fs::Dir, { to.handle = from.handle; });
DefineNilValue(iro::fs::Dir, {nullptr}, { return x.handle == nullptr; });

#endif // _iro_dir_h
