#include "io.h"

#include "stdlib.h"
#include "stdio.h"
#include "../memory/memory.h"

// TODO(sushi) platform layers
#if IRO_LINUX
#include "fcntl.h"
#include "sys/stat.h"
#include "unistd.h"
#endif // #if IRO_LINUX

#include "sys/stat.h"

#include "assert.h"

namespace iro::io
{

/* ------------------------------------------------------------------------------------------------
 */
void Memory::growIfNeeded(s64 wanted_space)
{
  if (space - len >= wanted_space)
    return;

  while (space - len < wanted_space)
    space *= 2;

  ptr = (u8*)allocator->reallocate(ptr, space);
}

/* ------------------------------------------------------------------------------------------------
 */
b8 Memory::open(s32 initial_space, mem::Allocator* allocator)
{
  this->allocator = allocator;

  if (flags.test(Flag::Open))
    return true;

  flags.set(Flag::Writable);
  flags.set(Flag::Readable);

  space = initial_space + 1;
  pos = len = 0;
  ptr = (u8*)allocator->allocate(space);
  ptr[initial_space] = 0;

  if (ptr != nullptr)
  {
    flags.set(Flag::Open);
    return true;
  }
  return false;
}

/* ------------------------------------------------------------------------------------------------
 */
void Memory::close()
{
  if (!isOpen())
    return;

  allocator->free(ptr);
  ptr = nullptr;
  len = space = 0;
  pos = 0;

  flags.unset(Flag::Open);

  *this = nil;
}

/* ------------------------------------------------------------------------------------------------
 */
Bytes Memory::reserve(s32 wanted_space)
{
  growIfNeeded(wanted_space+1);
  
  return {ptr + len, u64(wanted_space)};
}

/* ------------------------------------------------------------------------------------------------
 */
void Memory::commit(s32 committed_space)
{
  assert(len + committed_space <= space);
  len += committed_space;
  ptr[len] = 0;
}

/* ------------------------------------------------------------------------------------------------
 */
s64 Memory::write(Bytes slice)
{
  growIfNeeded(slice.len+1);

  mem::copy(ptr + len, slice.ptr, slice.len);
  len += slice.len;

  ptr[len] = 0;

  return slice.len;
}

/* ------------------------------------------------------------------------------------------------
 */
s64 Memory::read(Bytes outbuffer)
{
  if (pos == len)
    return 0;

  s64 bytes_remaining = len - pos;
  s64 bytes_to_read = 
    (bytes_remaining > outbuffer.len ? outbuffer.len : bytes_remaining);
  mem::copy(outbuffer.ptr, ptr + pos, bytes_to_read);
  pos += bytes_to_read;
  return bytes_to_read;
}

/* ------------------------------------------------------------------------------------------------
 */
s64 Memory::readFrom(s64 pos, Bytes slice)
{
  assert(pos < len);

  s64 bytes_remaining = len - pos;
  s64 bytes_to_read = (bytes_remaining > slice.len ? slice.len : bytes_remaining);
  mem::copy(slice.ptr, ptr + pos, bytes_to_read);
  return bytes_to_read;
}

/* ------------------------------------------------------------------------------------------------
 */
s64 Memory::consume(io::IO* io, u32 chunk_size)
{
  s64 sum = 0;
  for (;;)
  {
    Bytes reserved = reserve(chunk_size);

    s64 written = io->read(reserved);
    if (written == 0)
      return sum;

    sum += written;

    commit(written);
  }
}

/* ------------------------------------------------------------------------------------------------
 */
Memory::Rollback Memory::createRollback()
{
  return len;
}

/* ------------------------------------------------------------------------------------------------
 */
void Memory::commitRollback(Rollback rollback)
{
  len = rollback;
  rewind();
}

}
