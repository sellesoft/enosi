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
 */ 
Timespec clock_realtime();

/* ------------------------------------------------------------------------------------------------ clock_monotonic
 */ 
Timespec clock_monotonic();

} // namespace iro::platform

#endif
