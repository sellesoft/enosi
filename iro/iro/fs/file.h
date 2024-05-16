/*
 *  API for interacting with files in the filesystem.
 */

#ifndef _iro_file_h
#define _iro_file_h

#include "../common.h"
#include "../unicode.h"
#include "../flags.h"
#include "../time/time.h"

#include "path.h"

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

}

// semantics definitions
DefineMove(iro::fs::File, { to.handle = from.handle; to.path = move(from.path); });
DefineNilValue(iro::fs::File, {}, { return x.handle == -1; });
DefineScoped(iro::fs::File, { x.close(); });

DefineNilValue(iro::fs::FileInfo, {iro::fs::FileKind::Invalid}, { return x.kind == iro::fs::FileKind::Invalid; });

#endif
