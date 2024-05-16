/*
 *  Common filesystem stuff.
 */

#ifndef _iro_fs_h
#define _iro_fs_h

#include "move.h"
#include "path.h"
#include "containers/array.h"
#include "containers/stackarray.h"
#include "io/format.h"
#include "concepts"
#include "time/time.h"
#include "memory/bump.h"
#include "nil.h"
#include "scoped.h"

namespace iro::fs
{

struct FileInfo;

/* ================================================================================================ OpenFlag
 *  File opening behaviors supported by iro.
 */
enum class OpenFlag
{
	Read,
	Write,

	// Opens file in append-mode, eg. the file cursor starts at the end.
	Append,    
	// Create the file if it doesnt already exist.
	Create,    
	// If the file exists, is a regular file, and we have write access, 
	// it will be truncated to length 0 on open.
	Truncate,  
	// Fail to open if the file already exists.
	Exclusive, 
	// Do not follow symbolic links.
	NoFollow,  
	// Do not block on read/write requests to the opened file.
	NoBlock,   
};
typedef iro::Flags<OpenFlag> OpenFlags;

DefineFlagsOrOp(OpenFlags, OpenFlag);

/* ================================================================================================ FileKind
 */
enum class FileKind
{
	Invalid,
	Unknown,
	NotFound,
	Regular,
	Directory,
	SymLink,
	Block,
	Character,
	FIFO,
	Socket,
};

/* ================================================================================================ fs::File
 */
struct File : public io::IO
{
	typedef u64 Handle;

	// A handle to this file if opened.
	Handle handle = -1;

	Path path = nil;

	static File from(str path, OpenFlags flags, mem::Allocator* allocator = &mem::stl_allocator);
	static File from(Path path, OpenFlags flags, mem::Allocator* allocator = &mem::stl_allocator);
	static File from(Moved<Path> path, OpenFlags flags);

	[[deprecated("File cannot be opened without OpenFlags!")]]
	b8 open() override { assert(!"File cannot be opened without OpenFlags!"); return false; }

	// Opens the file specified by 'path'.
	b8 open(Moved<Path> path, OpenFlags flags);

	void close() override;

	s64 write(Bytes bytes) override;
	s64 read(Bytes bytes) override;

	static File stdout() { return fromFileDescriptor(1, "stdout"_str, OpenFlag::Write); }
	static File stderr() { return fromFileDescriptor(2, "stderr"_str, OpenFlag::Write); }
	static File stdin()  { return fromFileDescriptor(0, "stdin"_str, OpenFlag::Read); }

	static File fromFileDescriptor(u64 fd, OpenFlags flags);
	static File fromFileDescriptor(u64 fd, str name, OpenFlags flags, mem::Allocator* allocator = &mem::stl_allocator);
	static File fromFileDescriptor(u64 fd, Moved<Path> name, OpenFlags flags);

	// Returns a File::Info containing various information about this file.
	FileInfo getInfo();
};

/* ================================================================================================ fs::FileInfo
 */
struct FileInfo
{
	FileKind kind;

	u64 byte_size;

	TimePoint last_access_time;
	TimePoint last_modified_time;
	TimePoint last_status_change_time;
	TimePoint birth_time;

	static FileInfo of(str path);
	static FileInfo of(File file);
};

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
// The callback controls the walk by returning 'Stop', 'Next', or 'Recurse'.
void walk(str path, DirWalkCallback auto f, mem::Allocator* allocator = &mem::stl_allocator);

enum class DirGlobResult
{
	Stop,
	Continue,
};

template<typename F>
concept DirGlobCallback = requires(F f, FileKind k, str s)
{ { f(s, k) } -> std::convertible_to<DirGlobResult>; };

void glob(str pattern, DirGlobCallback auto f, mem::Allocator* allocator = &mem::stl_allocator);

}


// semantics definitions
DefineMove(iro::fs::File, { to.handle = from.handle; to.path = move(from.path); });
DefineNilValue(iro::fs::File, {}, { return x.handle == -1; });
DefineScoped(iro::fs::File, { x.close(); });

DefineMove(iro::fs::Dir, { to.handle = from.handle; });
DefineNilValue(iro::fs::Dir, {nullptr}, { return x.handle == nullptr; });

DefineNilValue(iro::fs::FileInfo, {iro::fs::FileKind::Invalid}, { return x.kind == iro::fs::FileKind::Invalid; });



/* -= -= -= -= -= -= -= -= -= -= -= -= -= -= -= -= -= -= -= -= -= -= -= -= -= -= -= -= -= -= -= -=  Templated implementations
 * TODO(sushi) try and move this shit elsewhere. Maybe make them not recursive so they dont 
 *             actually have to pass the templated function pointer like they are.
   =- =- =- =- =- =- =- =- =- =- =- =- =- =- =- =- =- =- =- =- =- =- =- =- =- =- =- =- =- =- =- */



namespace iro::fs
{

namespace __internal
{

/* ------------------------------------------------------------------------------------------------ walkForReal
 */
b8 walkForReal(Path& path, DirWalkCallback auto f)
{
	auto dir = Dir::open(path);

	// if were starting in cwd we dont want it to appear in the path we give
	// so just clear it 
	// TODO(sushi) this is kinda dumb
	if (path.isCurrentDirectory())
		path.clear();

	if (dir == nil)
		return false;

	defer { dir.close(); };

	// Path we offer to the user via the callback, which they can move 
	// elsewhere if they want to take it.
	MayMove<Path> user_path;
	defer { if (!user_path.wasMoved()) user_path.destroy(); };

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
			user_path.clear();
			user_path.append(path.buffer.asStr());
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

/* ------------------------------------------------------------------------------------------------ walk
 */
void walk(str pathin, DirWalkCallback auto f, mem::Allocator* allocator)
{
	auto path = scoped(Path::from(pathin, allocator));
	__internal::walkForReal(path, f);
}

/* ------------------------------------------------------------------------------------------------ glob
 *  TODO(sushi) move the bulk of this to its own file as well as walk's implementation.
 *              It sucks using templates causes this.
 *
 *  This is extremely memory inefficient.
 */ 
void glob(str pattern, DirGlobCallback auto f)
{
	mem::Bump allocator; // yeah
	if (!allocator.init())
		return;
	defer { allocator.deinit(); };
}

}



#endif // _iro_fs_h
