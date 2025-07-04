/*
 *  API for interacting with files in the filesystem.
 */

#ifndef _iro_File_h
#define _iro_File_h

#include "../Common.h"
#include "../Unicode.h"
#include "../Flags.h"
#include "../time/Time.h"

#include "Path.h"

#undef stdin
#undef stdout
#undef stderr

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

  COUNT
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

/* ============================================================================
 *  Arguments and returns from File::poll
 */
enum class PollEvent
{
  In,  // There is data to read, so a call to read() will not block.
  Out, // Writing now will not block.

  // These events are only returned and have no effect on the request.
  Err,     // Error occured.
  HangUp,  // Whatever was there isnt anymore.
  Invalid, // Invalid request.

  COUNT
};
typedef Flags<PollEvent> PollEventFlags;

DefineFlagsOrOp(PollEventFlags, PollEvent);

/* ============================================================================
 */
struct File : public io::IO
{
  typedef u64 Handle;

  // A handle to this file if opened.
  Handle handle = -1;

  Path path = nil;

  OpenFlags openflags = {};

  b8 is_pty = false; // TODO(sushi) put somewhere better later

  static File from(
      String path, OpenFlags flags, 
      mem::Allocator* allocator = &mem::stl_allocator);

  static File from(
      Path path, OpenFlags flags, 
      mem::Allocator* allocator = &mem::stl_allocator);

  static File from( Moved<Path> path, OpenFlags flags);

  static b8 copy(String dst, String src);
  static b8 rename(String dst, String src);
  static b8 unlink(String path);

  static b8 exists(String path);

  [[deprecated("File cannot be opened without OpenFlags!")]]
  b8 open() override 
    { assert(!"File cannot be opened without OpenFlags!"); return false; }

  // Opens the file specified by 'path'.
  b8 open(Moved<Path> path, OpenFlags flags);

  void close() override;

  s64 write(Bytes bytes) override;
  s64 read(Bytes bytes) override;

  static File fromFileDescriptor(u64 fd, OpenFlags flags);
  static File fromFileDescriptor(
      u64 fd, String name, OpenFlags flags, 
      mem::Allocator* allocator = &mem::stl_allocator);
  static File fromFileDescriptor(u64 fd, Moved<Path> name, OpenFlags flags);

  b8 poll(PollEventFlags* flags);

  // Test if this file refers to a terminal.
  b8 isatty();

  // Returns a File::Info containing various information about this file.
  FileInfo getInfo();

  DefineNilTrait(File, {}, x.handle == -1);
  DefineScopedTrait(File, { x.close(); });
  DefineMoveTrait(File, 
    { 
      to.handle = from.handle;  
      to.path = move(from.path);
    });
};

#if IRO_LINUX
inline File stdout = 
  File::fromFileDescriptor(1, "stdout"_str, OpenFlag::Write);
inline File stderr = 
  File::fromFileDescriptor(2, "stderr"_str, OpenFlag::Write);
inline File stdin = 
  File::fromFileDescriptor(0, "stdin"_str, OpenFlag::Read);
#elif IRO_WIN32
// NOTE(sushi) these are defined in File_Win32.cpp
extern File stdout;
extern File stderr;
extern File stdin;
#endif

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

  static FileInfo of(String path);
  static FileInfo of(File file);

  DefineNilTrait(FileInfo, {FileKind::Invalid}, x.kind == FileKind::Invalid);
};

}

#endif
