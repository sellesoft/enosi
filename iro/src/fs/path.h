/*
 *  Path to some File in the filesystem. This is just a lexical representation, the file named may or may not 
 *  exist.
 *
 *  Provides helpers to manipulate and query information about whatever is at the given path.
 *
 *  Paths must use '/' as the separator.
 *  TODO(sushi) normalize paths when I start getting stuff to work on Windows
 */

#ifndef _iro_path_h
#define _iro_path_h

#include "common.h"
#include "unicode.h"
#include "io/io.h"
#include "io/format.h"
#include "move.h"

namespace iro::fs
{

struct File;

struct Path
{
	io::Memory buffer; // dynamic buffer for now, maybe make static later

	// Makes a new Path from the given string.
	static Path from(str s = {}, mem::Allocator* allocator = &mem::stl_allocator);

	b8   init(mem::Allocator* allocator = &mem::stl_allocator) { return init({}, allocator); }
	b8   init(str s, mem::Allocator* allocator = &mem::stl_allocator);
	void destroy();

	Path copy();

	void append(io::Formattable auto... args) { io::formatv(&buffer, args...); }

	void makeDir() { if (buffer.buffer[buffer.len-1] != '/') append('/'); }

	// Returns the final component of this path eg.
	// src/fs/path.h -> path.h
	// This is a VIEW! So if you modify this path
	// the given str is no longer valid.
	str basename();

	// Helpers for querying information about the file at this path

	b8 exists();

	b8 isRegularFile();
	b8 isDirectory();

	// Returns if this path matches the given pattern
	// TODO(sushi) write up the specification of patterns that can be used here
	//             its just Linux shell globbing 
	b8 matches(str pattern);
};

}

DefineMove(iro::fs::Path, { to.buffer = move(from.buffer); });
DefineNilValue(iro::fs::Path, {Nil()}, { return x.buffer == Nil(); });

#endif // _iro_path_h
