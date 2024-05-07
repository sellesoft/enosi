/*
 *  Common filesystem stuff.
 */

#ifndef _iro_fs_h
#define _iro_fs_h

#include "path.h"
#include "containers/array.h"
#include "concepts"

namespace iro::fs
{

/* 
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

/* ================================================================================================ fs::File
 */
struct File : public io::IO
{
	// Opaue platform specific handle to this file.
	typedef void* Handle;

	enum class Kind
	{
		None,
		NotFound,
		Regular,
		Directory,
		SymLink,
		Block,
		Character,
		FIFO,
		Socket,
		Unknown,
	};

	// Information regarding a file.
	struct Info
	{
		Kind kind;
		u64 owner_uid;
		u64 owner_gid;
		u64 size; // in bytes
		
	};

	// A handle to this file if opened.
	Handle handle;

	Kind kind;
	str path;

	[[deprecated("File cannot be opened without OpenFlags!")]]
	b8 open() override { assert(!"File cannot be opened without OpenFlags!"); return false; }

	// Opens the file specified by 'path'. Fails if 'path' is empty.
	b8 open(OpenFlags flags);
	b8 open(str path, OpenFlags flags);

	void close() override;

	s64 write(Bytes bytes) override;
	s64 read(Bytes bytes) override;

	static File fromStdout();
	static File fromStderr();
	static File fromStdin();
};

/* ================================================================================================ fs::Dir
 *  Type used for streaming directory iteration.
 */
struct Dir
{
	// Platform-dependent handle to the directory being iterated.
	void* handle;

	static Dir open(str path);
	void close();

	// Writes into 'file' the next entry in this directory 
	// until we run out. Returns false when we run out of 
	// entries.
	b8 next(File* file);
};

enum class DirWalkResult
{
	Stop,
	Next,
	Recurse,
};

template<typename F>
concept DirWalkPathCallback = requires(F f, File::Kind k, str s)
{ { f(s, k) } -> std::convertible_to<DirWalkResult>; };

template<typename F>
concept DirWalkFileCallback = requires(F f, File file)
{ { f(file) } -> std::convertible_to<DirWalkResult>; };

enum class DirGlobResult
{
	Stop,
	Continue,
};

template<typename F>
concept DirGlobPathCallback = requires(F f, File::Kind k, str s)
{ { f(s, k) } -> std::convertible_to<DirGlobResult>; };

template<typename F>
concept DirGlobFileCallback = requires(F f, File file)
{ { f(file) } -> std::convertible_to<DirGlobResult>; };


// Walk the given directory using callback 'f' starting at 'path'.
// The callback controls the walk by returning 'Stop', 'Next', or 'Recurse'.
void walk(str path, DirWalkPathCallback auto f);
void walk(str path, DirWalkFileCallback auto f);

void glob(str pattern, DirGlobPathCallback auto f);
void glob(str pattern, DirGlobFileCallback auto f);

}

#endif // _iro_fs_h
