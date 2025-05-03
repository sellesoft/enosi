/*
 *  Collection of all platform specific functionality used within iro.
 *
 *  Implementations of these functions for supported platforms should go in
 *  their respective Platform_*.cpp files.
 *
 *  These functions are meant to be used internally, but its probably fine to
 *  use them directly.
 *
 */

#ifndef _iro_Platform_h
#define _iro_Platform_h

#include "Common.h"
#include "Unicode.h"
#include "containers/Slice.h"

#include "fs/FileSystem.h"
#include "Process.h"

namespace iro::platform
{

/* ----------------------------------------------------------------------------
 *  Puts the calling thread to sleep for the given TimeSpan.
 *
 *  On Linux this uses 'usleep' which has a precision of microseconds.
 *  On Win32 this uses 'Sleep' which has a precision of milliseconds.
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
b8 open(fs::File::Handle* out_handle, String path, fs::OpenFlags flags);

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
b8 opendir(fs::Dir::Handle* out_handle, String path);

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
 *  Returns if the stdin pipe has data ready to read.
 */
b8 stdinHasData();

/* ----------------------------------------------------------------------------
 *  Fills out the given fs::FileInfo with information about the file at the
 *  given path.
 */
b8 stat(fs::FileInfo* out_info, String path);

/* ----------------------------------------------------------------------------
 *  Perform a basic check that a file exists at 'path'.
 *  'path' must be null-terminated.
 */
b8 fileExists(String path);

/* ----------------------------------------------------------------------------
 *  Copies the file at path 'src' to the path 'dst'. At the moment this always
 *  overwrites the file at 'dst' with 'src'. Maybe someone will add options
 *  later, maybe even me.
 */
b8 copyFile(String dst, String src);

/* ----------------------------------------------------------------------------
 *  Equivalent to 'unlink' on Linux. IDK how this will behave on Windows/Mac
 *  yet and so I'll put a better description here when I do.
 *
 *  This works on any kind of file except directories. Use 'rmdir' to delete
 *  those.
 */
b8 unlinkFile(String path);

/* ----------------------------------------------------------------------------
 *  Equivalent to 'rmdir' on Linux. The directory must be empty to be deleted.
 */
b8 removeDir(String path);

/* ----------------------------------------------------------------------------
 *  Create the directory at 'path'. If make_parents is true, any missing
 *  parent directories will be created as well.
 */
b8 makeDir(String path, b8 make_parents);

/* ----------------------------------------------------------------------------
 *  Creates a new process and writes its handle into 'out_handle'. If 'cwd' is
 *  not nil then the child process will chdir into it before starting the
 *  actual process.
 */
b8 processSpawn(
    Process::Handle* out_handle,
    String           file,
    Slice<String>    args,
    String           cwd,
    b8               non_blocking,
    b8               redirect_err_to_out);

/* ----------------------------------------------------------------------------
 *  Returns true if a Process has data to be read.
 */
b8 processHasOutput(Process::Handle h_process);

/* ----------------------------------------------------------------------------
 *  Returns true if a Process has data to be read over stderr.
 */
b8 processHasErrOutput(Process::Handle h_process);

/* ----------------------------------------------------------------------------
 *  Reads some amount of output from a process. Returns the number of bytes
 *  read, if any.
 */
u64 processRead(Process::Handle h_process, Bytes buffer);

/* ----------------------------------------------------------------------------
 *  Reads some amount of stderr output from a process. Returns the number of
 *  bytes read, if any.
 */
u64 processReadStdErr(Process::Handle h_process, Bytes buffer);

/* ----------------------------------------------------------------------------
 *  Writes some data to the given process. Returns number of bytes written.
 */
u64 processWrite(Process::Handle h_process, Bytes buffer);

/* ----------------------------------------------------------------------------
 *  Returns true if a Process has exited and its exit code if so.
 */
b8 processHasExited(Process::Handle h_process, s32* out_exit_code);

/* ----------------------------------------------------------------------------
 *  Cleans up handles held by a process. After this call, the given handle is
 *  no longer valid. If the process given is still running when this is called
 *  it is terminated with exit code 0.
 */
b8 processClose(Process::Handle h_process);

/* ----------------------------------------------------------------------------
 *  Converts the given path, that must exist, to a canonical path, that is with
 *  segments /./ and /../ evaluated and links followed.
 */
b8 realpath(fs::Path* path);

/* ----------------------------------------------------------------------------
 *  Returns the current working directory. This always allocates memory with
 *  the given allocator and needs to be freed later if the function is
 *  successful, so a Path is returned rather than a plain String.
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
b8 chdir(String path);

/* ----------------------------------------------------------------------------
 *  Changes the working directory of the program using a handle to a directory.
 *  This is primarily useful as a more efficient way to store directories that
 *  we want to return to as it avoids needing to store strings, and especially
 *  because it avoids using the awful cwd().
 *
 *  However, note that on Win32 we still have to extract the path to the given
 *  dir in order to chdir into it as Win32 does not provide an equivalent to
 *  Linux's fchdir().
 */
b8 chdir(fs::Dir::Handle dir);

/* ----------------------------------------------------------------------------
 *  Sets the terminal to be non-canonical, eg. disable buffering stdin until
 *  newlines. Returns an opaque pointer to saved terminal settings that must be
 *  restored when the program closes. If you don't do this it will leave the
 *  terminal in a bad state!!
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
b8 touchFile(String path);

/* ----------------------------------------------------------------------------
 *  Open a 'pseudo-terminal' eg. a file handle that masquerades as a normal
 *  terminal. This is useful when consuming input from a child process when
 *  you want that process to believe it is outputting to a real terminal
 *  (to preverse colors for instance).
 */
b8 openptty();

/* ----------------------------------------------------------------------------
 */
s32 getEnvVar(String name, Bytes buffer);

/* ----------------------------------------------------------------------------
 */
b8 setEnvVar(String name, String value);

/* ----------------------------------------------------------------------------
 */
[[noreturn]]
void exit(int status);

/* ----------------------------------------------------------------------------
 *  Check if the process is running under a debugger.
 */
b8 isRunningUnderDebugger();

/* ----------------------------------------------------------------------------
 *  Breaks the program for a debuggger.
 */
void debugBreak();

/* ----------------------------------------------------------------------------
 */
u64 getPid();

/* ----------------------------------------------------------------------------
 *  TODO(sushi) put these somewhere better later.
 */
u16 byteSwap(u16 x);
u32 byteSwap(u32 x);
u64 byteSwap(u64 x);

/* ----------------------------------------------------------------------------
 * Reserves pages of memory. This only reserves an address range.
 * Minimum size is OS-dependent, but 4 kilobytes is common.
 */
void* reserveMemory(u64 size);

/* ----------------------------------------------------------------------------
 * Commits pages of memory. This actually maps the reserved pointer to
 * physical memory.
 */
b8 commitMemory(void* ptr, u64 size);

/* ----------------------------------------------------------------------------
 * Reserves large pages of memory. This only reserves an address range.
 * Minimum size is CPU-dependent, but 2 megabytes is common.
 */
void* reserveLargeMemory(u64 size);

/* ----------------------------------------------------------------------------
 * Commits large pages of memory. This actually maps the reserved pointer to
 * physical memory.
 */
b8 commitLargeMemory(void* ptr, u64 size);

/* ----------------------------------------------------------------------------
 */
void decommitMemory(void* ptr, u64 size);

/* ----------------------------------------------------------------------------
 */
void releaseMemory(void* ptr, u64 size);

} // namespace iro::platform

#endif
