#include "file.h"

#include "../platform.h"
#include "../logger.h"

namespace iro::fs
{

static Logger logger = 
  Logger::create("iro.fs.file"_str, Logger::Verbosity::Notice);

/* ----------------------------------------------------------------------------
 */
File File::from(String path, OpenFlags flags, mem::Allocator* allocator)
{
  return from(move(Path::from(path, allocator)), flags);
}

File File::from(Path path, OpenFlags flags, mem::Allocator* allocator)
{
  return from(move(Path::from(path.buffer.asStr(), allocator)), flags);
}

File File::from(Moved<Path> path, OpenFlags flags)
{
  if (isnil(path))
  {
    ERROR("nil path given to File::from()\n");
    return nil;
  }

  File out = nil;

  if (!out.open(path, flags))
    return nil;

  return out;
}

/* ----------------------------------------------------------------------------
 */
b8 File::copy(String dst, String src)
{
  return platform::copyFile(dst, src);
}

/* ----------------------------------------------------------------------------
 */
b8 File::unlink(String path)
{
  return platform::unlinkFile(path);
}

/* ----------------------------------------------------------------------------
 */
b8 File::open(Moved<Path> path, OpenFlags flags)
{
  if (isOpen())
  {
    path.destroy();
    return false; 
  }

  if (!platform::open(&handle, path.buffer.asStr(), flags))
  {
    path.destroy();
    return false;
  }

  setOpen();

  if (flags.test(OpenFlag::Read))
    setReadable();

  if (flags.test(OpenFlag::Write))
    setWritable();

  openflags = flags;

  this->path = path;

  return true;
}

/* ----------------------------------------------------------------------------
 */
void File::close()
{
  if (!isOpen())
    return;

  // ugh
  if (handle > 2)
    platform::close(handle);
  
  // The path could have been moved for storage elsewhere.
  if (notnil(path))
    path.destroy();

  unsetOpen();
  unsetReadable();
  unsetWritable();

  *this = nil;
}

/* ----------------------------------------------------------------------------
 */
s64 File::write(Bytes bytes)
{
  if (!isOpen() ||
      !canWrite())
    return -1;

  return platform::write(
      handle, 
      bytes, 
      openflags.test(OpenFlag::NoBlock),
      is_pty);
}

/* ----------------------------------------------------------------------------
 */
s64 File::read(Bytes bytes)
{
  if (!isOpen() ||
      !canRead())
    return -1;

  return platform::read(
      handle, 
      bytes, 
      openflags.test(OpenFlag::NoBlock),
      is_pty);
}

/* ----------------------------------------------------------------------------
 */
b8 File::poll(PollEventFlags* flags)
{
  assert(handle and flags);

  if (!platform::poll(handle, flags))
    return false;
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 File::isatty()
{
  return platform::isatty(handle);
}

/* ----------------------------------------------------------------------------
 */
File File::fromFileDescriptor(u64 fd, OpenFlags flags)
{
  File out = {};
  out.handle = fd;

  DEBUG("creating File from file descriptor ", fd, "\n");
  SCOPED_INDENT;

  out.setOpen();

  if (flags.test(OpenFlag::Read))
    out.setReadable();

  if (flags.test(OpenFlag::Write))
    out.setWritable();

  if (flags.test(OpenFlag::NoBlock))
  {
    TRACE("setting as non-blocking\n");
    platform::setNonBlocking(out.handle);
  }

  out.path = nil;

  out.openflags = flags;

  return out;
}

File File::fromFileDescriptor(
    u64 fd, 
    String name, 
    OpenFlags flags, 
    mem::Allocator* allocator)
{
  return fromFileDescriptor(fd, move(Path::from(name, allocator)), flags);
}

File File::fromFileDescriptor(u64 fd, Moved<Path> name, OpenFlags flags)
{
  File out = fromFileDescriptor(fd, flags);
  out.path = name;
  return out;
}

/* ----------------------------------------------------------------------------
 */
FileInfo File::getInfo()
{
  return FileInfo::of(*this);
}

/* ----------------------------------------------------------------------------
 */
FileInfo FileInfo::of(String path)
{
  assert(notnil(path) && path.len);

  FileInfo out = {};
  if (!platform::stat(&out, path))
    return nil;
  return out;
}

FileInfo FileInfo::of(File file)
{
  return FileInfo::of(file.path.buffer.asStr());
}

}
