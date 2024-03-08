/*
 *
 *  Platform specific functions.
 *
 */

#ifndef _lake_platform_h
#define _lake_platform_h

#include "common.h"

/* ------------------------------------------------------------------------------------------------
 *  Uses mem.allocate so we can use mem.free to free.
 */
u8* read_file(str path);

/* ------------------------------------------------------------------------------------------------
 *  dir_next() copies a path into 'c' and returns the length of the path.
 */
typedef void* DirIter;

DirIter dir_iter(const char* path);
u32     dir_next(char* c, u32 maxlen, DirIter iter);

/* ------------------------------------------------------------------------------------------------
 */
b8 is_file(const char* path);
b8 is_dir(const char* path);

/* ------------------------------------------------------------------------------------------------
 *  Returns a timestamp in seconds.
 */
s64 modtime(const char* path);

#endif
