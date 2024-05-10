/*
 *  Common filesystem stuff.
 */

#ifndef _iro_fs_h
#define _iro_fs_h

#include "path.h"
#include "containers/array.h"
#include "containers/stackarray.h"
#include "io/format.h"
#include "concepts"
#include "time/time.h"
#include "memory/bump.h"
#include "move.h"
#include "nil.h"

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
	typedef void* Handle;

	// A handle to this file if opened.
	Handle handle = nullptr;

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

	static File fromStdout();
	static File fromStderr();
	static File fromStdin();

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

	static FileInfo invalid() { return {FileKind::Invalid}; }
	b8 isValid() { return kind != FileKind::Invalid; }

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

	// Open a directory stream using a file handle, if that handle represents a 
	// directory.
	static Dir open(File::Handle file_handle);

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
concept DirWalkCallback = requires(F f, Path& path)
{ { f(path) } -> std::convertible_to<DirWalkResult>; };

// Walk the given directory using callback 'f' starting at 'path'.
// The callback controls the walk by returning 'Stop', 'Next', or 'Recurse'.
void walk(str path, DirWalkCallback auto f);

enum class DirGlobResult
{
	Stop,
	Continue,
};

template<typename F>
concept DirGlobCallback = requires(F f, FileKind k, str s)
{ { f(s, k) } -> std::convertible_to<DirGlobResult>; };

void glob(str pattern, DirGlobCallback auto f);

}


DefineMove(iro::fs::File, { to.handle = from.handle; to.path = move(from.path); });
DefineNilValue(iro::fs::File, {}, { return x.handle == nullptr; });

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
b8 walkForReal(io::Memory* path, DirWalkCallback auto f)
{
	auto dir = Dir::open(path->asStr());

	if (dir == nil)
		return false;

	defer { dir.close(); };

	if (path->buffer[path->len-1] != '/')
		io::format(path, '/');

	for (;;)
	{
		auto rollback = path->createRollback();
		defer { path->commitRollback(rollback); };

		path->reserve(255);

		s64 len = dir.next({path->buffer + path->len, path->space - path->len});
		if (len < 0)
			return false;
		else if (len == 0)
			return true;
		
		path->commit(len);

		if (len == 1 && path->buffer[path->len-1] == '.' ||
			len == 2 && path->buffer[path->len-1] == '.' && path->buffer[path->len-2] == '.')
			continue;

		// create a Path the user may move() if they want to keep it 
		Path for_user = Path::from(path->asStr());
		auto result = f(path);
		if (for_user == nil)
			for_user.destroy();

		switch (result)
		{
		case DirWalkResult::Stop: 
			return false;
		
		case DirWalkResult::Next:
			break;

		case DirWalkResult::StepInto:
			io::format(path, '/');
			if (!walk_for_real(path, f))
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
void walk(str path, DirWalkCallback auto f)
{
	io::Memory path_accumulator; 
	path_accumulator.open(path.len);
	path_accumulator.write(path);
	__internal::walkForReal(&path_accumulator, f);
	path_accumulator.close();

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
