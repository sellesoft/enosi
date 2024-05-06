/*
 *  Dir utils
 */

#ifndef _iro_dir_h
#define _iro_dir_h

#include "common.h"
#include "unicode.h"
#include "concepts"

namespace iro::fs
{

enum class DirEachResult
{
	Stop,
	Next,
	Recurse,
};

template<typename F>
concept DirWalkCallback = requires(F f, str s)
{ { f(s) } -> std::convertible_to<DirEachResult>; };

enum class DirGlobResult
{
	Stop,
	Continue,
};

template<typename F>
concept DirGlobCallback = requires(F f, str s)
{ { f(s) } -> std::convertible_to<DirGlobResult>; };

struct Dir
{
	static void walk(str path, DirWalkCallback auto f);

	static void glob(str pattern, DirGlobCallback auto f);
};

}

#endif // _iro_dir_h
