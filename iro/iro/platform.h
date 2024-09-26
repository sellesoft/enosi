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
#include "process.h"

namespace iro::platform
{

/* ----------------------------------------------------------------------------
 *  Puts the calling thread to sleep for the given TimeSpan.
 *
 *  On Linux this uses 'usleep' which has a precision of microseconds. I 
 *  believe on Windows it will be the same.
 */
void sleep(TimeSpan time);

/* ----------------------------------------------------------------------------
 *  Open a file with the behavior defined by 'flags'. A platform specific 
 *  handle is written to 'out_handle'.
 *
 *  'path' must be null-terminated.
 *
 *  TODO(sushi) gather permission mode flags from windows and linux and pass 
 *              them here.
 */
b8 open(fs::File::Handle* out_handle, str path, fs::OpenFlags flags);

/* ----------------------------------------------------------------------------
 *  Closes a file opened by a previous call to open(). 
 */
b8 close(fs::File::Handle handle);

/* ----------------------------------------------------------------------------
 *  Reads data from a file opened by a previous call to open() into 'buffer'. 
 *  This is equivalent to Linux's read().
 *  If the file referred to by handle is set to not block on read/writes, set 
 *  non_blocking to true to suppress the error that normally appears (on 
 *  Linux, at least).
 *  If the file is a pty set 'is_pty' to prevent EIO from causing failure.
 *
 *  TODO(sushi) implement equivalents to Linux's pread/pwrite and readv/writev.
 */
s64 read(
    fs::File::Handle handle, 
    Bytes buffer, 
    b8 non_blocking = false,
    b8 is_pty = false);

/* ----------------------------------------------------------------------------
 *  Writes data to a file opened by a previous call to open(). In the same 
 *  manner as read(), this is equivalent to Linux's write().
 *  If the file referred to by handle is set to not block on read/writes, set 
 *  non_blocking to true to suppress the error that normally appears 
 *  (on Linux, at least).
 *  If the file is a pty set 'is_pty' to prevent EIO from causing failure.
 */
s64 write(
    fs::File::Handle handle, 
    Bytes buffer, 
    b8 non_blocking = false,
    b8 is_pty = false);

/* ----------------------------------------------------------------------------
 */
b8 poll(fs::File::Handle handle, fs::PollEventFlags* flags);

/* ----------------------------------------------------------------------------
 *  Returns true if the given file handle refers to a terminal.
 */
b8 isatty(fs::File::Handle handle);

/* ----------------------------------------------------------------------------
 *  Sets the given file handle as non-blocking, eg. read will not block if 
 *  there is no buffered data take and write will not block until something 
 *  consumes the written data.
 */
b8 setNonBlocking(fs::File::Handle handle);

/* ============================================================================
 *  Data returned by clock functions.
 */
struct Timespec
{
  u64 seconds;
  u64 nanoseconds;
};

/* ----------------------------------------------------------------------------
 *  Returns a timespec using the systems realtime clock
 */ 
Timespec clock_realtime();

/* ----------------------------------------------------------------------------
 *  Returns a timespec using the systems monotonic clock
 */ 
Timespec clock_monotonic();

/* ----------------------------------------------------------------------------
 *  Opens a directory stream.
 *
 *  'path' must be null terminated.
 */ 
b8 opendir(fs::Dir::Handle* out_handle, str path);

/* ----------------------------------------------------------------------------
 *  Opens a directory stream using an already existing file handle, assuming 
 *  the handle is for a directory.
 */
b8 opendir(fs::Dir::Handle* out_handle, fs::File::Handle file_handle);

/* ----------------------------------------------------------------------------
 *  Closes the given directory stream.
 */
b8 closedir(fs::Dir::Handle handle);

/* ----------------------------------------------------------------------------
 *  Writes into 'buffer' the name of the next file in the directory referred to 
 *  by 'handle' and returns the number of bytes written. Returns 0 when 
 *  finished and -1 if an error occurs.
 */
s64 readdir(fs::Dir::Handle handle, Bytes buffer);

/* ----------------------------------------------------------------------------
 *  Fills out the given fs::FileInfo with information about the file at the 
 *  given path.
 */
b8 stat(fs::FileInfo* out_info, str path);

/* ----------------------------------------------------------------------------
 *  Perform a basic check that a file exists at 'path'.
 *  'path' must be null-terminated.
 */
b8 fileExists(str path);

/* ----------------------------------------------------------------------------
 *  Copies the file at path 'src' to the path 'dst'. At the moment this always 
 *  overwrites the file at 'dst' with 'src'. Maybe someone will add options 
 *  later, maybe even me.
 */
b8 copyFile(str dst, str src);

/* ----------------------------------------------------------------------------
 *  Equivalent to 'unlink' on Linux. IDK how this will behave on Windows/Mac 
 *  yet and so I'll put a better description here when I do.
 *
 *  This works on any kind of file except directories. Use 'rmdir' to delete 
 *  those.
 */
b8 unlinkFile(str path);

/* ----------------------------------------------------------------------------
 *  Equivalent to 'rmdir' on Linux. The directory must be empty to be deleted.
 */
b8 removeDir(str path);

/* ----------------------------------------------------------------------------
 *  Create the directory at 'path'. If make_parents is true, any missing 
 *  parent directories will be created as well.
 */
b8 makeDir(str path, b8 make_parents);

/* ----------------------------------------------------------------------------
 *  Creates a new process and writes its handle into 'out_handle'. If 'cwd' is 
 *  not nil then the child process will chdir into it before starting the 
 *  actual process.
 */
b8 processSpawn(
    Process::Handle* out_handle, 
    str              file, 
    Slice<str>       args, 
    Process::Stream  streams[3], 
    str              cwd);

/* ----------------------------------------------------------------------------
 *  Performs a check on the process referred to by 'handle' and determines if 
 */
b8 processSpawnPTY(
    Process::Handle* out_handle,
    str              file,
    Slice<str>       args,
    fs::File*        stream);

/* ----------------------------------------------------------------------------
 *  Performs a check on the process referred to by 'handle' and determines if 
 *  it has existed or not. If it has, the process's exit code is written into 
 *  'out_exit_code' and Exited is returned. Otherwise StillRunning is returned 
 *  or Error if an error occurs.
 *
 *  Its probably safe to assume that if Error is returned, the process is 
 *  terminated. BUT IDK!
 */
enum class ProcessCheckResult
{
  Error,
  Exited,
  StillRunning,
};

ProcessCheckResult processCheck(Process::Handle handle, s32* out_exit_code);

/* ----------------------------------------------------------------------------
 *  Converts the given path, that must exist, to a canonical path, that is with 
 *  segments /./ and /../ evaluated and links followed.
 */
b8 realpath(fs::Path* path);

/* ----------------------------------------------------------------------------
 *  Returns the current working directory. This always allocates memory with 
 *  the given allocator and needs to be freed later if the function is 
 *  successful, so a Path is returned rather than a plain str.
 *
 *  NOTE that this function may need to try to allocate a buffer large enough 
 *  to fit the cwd multiple times! If the given allocator does not support 
 *  realloc/free the memory needs to be freeable some other way! This is 
 *  probably not the greatest way to support this function and it should be 
 *  changed later. Maybe it can take an io::IO* ? If only POSIX specified
 *  a function to just get the length of the cwd path!!!
 */ 
fs::Path cwd(mem::Allocator* allocator = &mem::stl_allocator);

/* ----------------------------------------------------------------------------
 *  Changes the working directory of the program to the one specified at 
 *  'path'.
 */
b8 chdir(str path);

/* ----------------------------------------------------------------------------
 *  Sets the terminal to be non-canonical, eg. disable buffering stdin until 
 *  newlines. Returns an opaque pointer to saved terminal settings that must be 
 *  restored when the program closes. If you don't do this it will leave the 
 *  terminal in a bad state!!
 *
 *  Use this when implementing for windows.
 *  https://gist.github.com/cloudwu/96ec4d6636d65b7974b633e01d97a1f9
 */
typedef void* TermSettings;

TermSettings termSetNonCanonical(
    mem::Allocator* allocator = &mem::stl_allocator);

/* ----------------------------------------------------------------------------
 *  Restores the settings given by a previous call to termSetNonCanonical.
 */
void termRestoreSettings(
    TermSettings settings, 
    mem::Allocator* allocator = &mem::stl_allocator);

/* ----------------------------------------------------------------------------
 *  Update the modification time of the file at the given path to the current 
 *  time. This does not create the file.
 *
 *  Returns false on failure.
 */
b8 touchFile(str path);

/* ----------------------------------------------------------------------------
 *  Open a 'pseudo-terminal' eg. a file handle that masquerades as a normal 
 *  terminal. This is useful when consuming input from a child process when
 *  you want that process to believe it is outputting to a real terminal
 *  (to preverse colors for instance).
 */ 
b8 openptty();

/* ----------------------------------------------------------------------------
 *  Check if the process is running under a debugger.
 */
b8 isRunningUnderDebugger();

/* ----------------------------------------------------------------------------
 *  Breaks the program for a debuggger.
 */
void debugBreak();

/* ----------------------------------------------------------------------------
 *  TODO(sushi) put these somewhere better later.
 */
 u16 byteSwap(u16 x);
 u32 byteSwap(u32 x);
 u64 byteSwap(u64 x);

} // namespace iro::platform

#endif
