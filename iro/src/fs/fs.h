/*
 *  Common filesystem stuff.
 */

#ifndef _iro_fs_h
#define _iro_fs_h

#include "path.h"
#include "containers/array.h"

#include "filesystem"

namespace iro::fs
{

enum class FileType
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

enum class DirEachResult
{
	Stop,
	Next,
	Recurse,
};

template<typename F>
concept DirWalkCallback = requires(F f, FileType t, str s)
{ { f(s, t) } -> std::convertible_to<DirEachResult>; };

enum class DirGlobResult
{
	Stop,
	Continue,
};

template<typename F>
concept DirGlobCallback = requires(F f, FileType t, str s)
{ { f(s, t) } -> std::convertible_to<DirGlobResult>; };


// Walk the given directory using callback 'f' starting at 'path'.
// The callback controls the walk by returning 'Stop', 'Next', or 'Recurse'.
void walk(str path, DirWalkCallback auto f);

void glob(str pattern, DirGlobCallback auto f);


}

#endif // _iro_fs_h
