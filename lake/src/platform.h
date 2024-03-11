/*
 *
 *  Platform specific functions.
 *
 */

#ifndef _lake_platform_h
#define _lake_platform_h

#include "common.h"

extern "C"
{

/* ------------------------------------------------------------------------------------------------
 *  Uses mem.allocate so we can use mem.free to free.
 */
u8* read_file(str path);

/* ------------------------------------------------------------------------------------------------
 *  dir_next() copies a path into 'c' and returns the length of the path.
 *  returns -1 if the end of the directory stream has been reached.
 */
typedef void* DirIter;

DirIter dir_iter(const char* path);
s32     dir_next(char* c, u32 maxlen, DirIter iter);

/* ------------------------------------------------------------------------------------------------
 */
struct Glob 
{
	s64 n_paths;
	u8** paths;

	void* handle;
};

Glob glob_create(const char* pattern);
void glob_destroy(Glob glob);

/* ------------------------------------------------------------------------------------------------
 */
b8 path_exists(str path);

/* ------------------------------------------------------------------------------------------------
 */
b8 is_file(const char* path);
b8 is_dir(const char* path);

/* ------------------------------------------------------------------------------------------------
 *  Returns a timestamp in seconds.
 */
s64 modtime(const char* path);

}

#endif
