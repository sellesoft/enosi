/*
 *  Collection of all platform specific functionality used within iro.
 *
 *  Implementations of these functions for supported platforms should go in their
 *  respective platform_*.cpp files.
 *
 *  These functions are meant to be used internally, but its probably fine to use
 *  them directly.
 *
 */

#ifndef _iro_platform_h
#define _iro_platform_h

#include "common.h"
#include "unicode.h"
#include "containers/slice.h"

#include "fs/fs.h"

namespace iro::platform
{

/* ------------------------------------------------------------------------------------------------ open
 *  Open a file with the behavior defined by 'flags'. A platform specific handle is written to
 *  'out_handle'.
 *
 *  'path' must be null-terminated.
 *
 *  TODO(sushi) gather permission mode flags from windows and linux and pass them here 
 */
b8 open(fs::File::Handle* out_handle, str path, fs::OpenFlags flags);

/* ------------------------------------------------------------------------------------------------ close
 *  Closes a file opened by a previous call to open(). 
 */
b8 close(fs::File::Handle handle);

/* ------------------------------------------------------------------------------------------------ read
 *  Reads data from a file opened by a previous call to open() into 'buffer'. This is equivalent to 
 *  Linux's read().
 *
 *  TODO(sushi) implement equivalents to Linux's pread/pwrite and readv/writev.
 */
s64 read(fs::File::Handle handle, Bytes buffer);

/* ------------------------------------------------------------------------------------------------ write
 *  Writes data to a file opened by a previous call to open(). In the same manner as read(), this
 *  is equivalent to Linux's write().
 */
s64 write(fs::File::Handle handle, Bytes buffer);


/* ================================================================================================ Timespec
 *  Data returned by clock functions.
 */
struct Timespec
{
	u64 seconds;
	u64 nanoseconds;
};

/* ------------------------------------------------------------------------------------------------ clock_realtime
 *  Returns a timespec using the systems realtime clock
 */ 
Timespec clock_realtime();

/* ------------------------------------------------------------------------------------------------ clock_monotonic
 *  Returns a timespec using the systems monotonic clock
 */ 
Timespec clock_monotonic();

/* ------------------------------------------------------------------------------------------------ opendir
 *  Opens a directory stream.
 *
 *  'path' must be null terminated.
 */ 
b8 opendir(fs::Dir::Handle* out_handle, str path);

/* ------------------------------------------------------------------------------------------------ opendir
 *  Opens a directory stream using an already existing file handle, assuming the handle is for 
 *  a directory.
 */
b8 opendir(fs::Dir::Handle* out_handle, fs::File::Handle file_handle);

/* ------------------------------------------------------------------------------------------------ closedir
 *  Closes the given directory stream.
 */
b8 closedir(fs::Dir::Handle handle);

/* ------------------------------------------------------------------------------------------------ readdir
 *  Writes into 'buffer' the name of the next file in the directory referred to by 'handle' and
 *  returns the number of bytes written. Returns 0 when finished and -1 if an error occurs.
 */
s64 readdir(fs::Dir::Handle handle, Bytes buffer);

/* ------------------------------------------------------------------------------------------------ stat
 *  Fills out the given fs::FileInfo with information about the file at the given path.
 */
b8 stat(fs::FileInfo* out_info, str path);

/* ------------------------------------------------------------------------------------------------ file_exists
 *  Perform a basic check that a file exists at 'path'.
 *
 *  'path' must be null-terminated.
 */
b8 file_exists(str path);

} // namespace iro::platform

#endif
