/*
 *  IO interface and various api for generic transmission of data
 */

#ifndef _iro_IO_h
#define _iro_IO_h

#include "../Common.h"
#include "../Unicode.h"
#include "../Flags.h"

#include "../traits/Nil.h"
#include "../traits/Move.h"
#include "../memory/Allocator.h"
#include "../memory/Memory.h"
#include "../containers/Slice.h"
#include "../containers/ArrayFuncs.h"

#include "assert.h"

namespace iro
{

namespace io
{

enum class Flag
{
  Open,
  Writable,
  Readable,

  COUNT
};
typedef iro::Flags<Flag> Flags; 

/* ============================================================================
 *  Base interface for input/output.
 *
 *  TODO(sushi) rework this into a Stream type and this file into more general
 *              I/O stuff.
 */
struct IO
{
  Flags flags = {};

  // Writes the contents of 'slice' into this IO and returns the number of 
  // bytes written.
  //
  // Note that it is currently assumed that all IO is copy on write. A large 
  // portion of the use of this function passes in pointers to stack.
  virtual s64 write(Bytes slice) = 0;

  // Helper for directly writing out the data of a type.
  void writeData(auto&& x)
  {
    write({(u8*)&x, sizeof(x)});
  }

  // Reads the contents of this IO into the buffer at 'buffer.ptr' up to 
  // 'buffer.len' and returns the number of bytes read.
  virtual s64 read(Bytes buffer) = 0;

  virtual b8   open()  { flags.set(Flag::Open); return true; }
  virtual void close() { flags.unset(Flag::Open); }
  virtual b8   flush() { return true; }

  // implementable on IO that can seek
  // reads into buffer starting at 'pos'
  virtual s64 readFrom(s64 pos, Bytes buffer) { assert(false); return 0; }

  b8 isOpen()   { return flags.test(Flag::Open); }
  b8 canRead()  { return flags.test(Flag::Readable); }
  b8 canWrite() { return flags.test(Flag::Writable); }

  template<typename T>
  s64 write(T* v)
  {
    return write(Bytes{ (u8*)v, sizeof(T) });
  }


  template<typename T>
  s64 read(T* v)
  {
    return read(Bytes{(u8*)v, sizeof(T)});
  }

protected:

  // Helpers for use internally, especially for things that define their own 
  // 'flags' like FileDescriptor.
  void setOpen()     { flags.set(Flag::Open); }
  void setWritable() { flags.set(Flag::Writable); }
  void setReadable() { flags.set(Flag::Readable); }

  void unsetOpen()     { flags.unset(Flag::Open); }
  void unsetWritable() { flags.unset(Flag::Writable); }
  void unsetReadable() { flags.unset(Flag::Readable); }
};

/* ============================================================================
 *  IO around a memory buffer that may grow.
 */
struct Memory : public IO
{
  u8* ptr = nullptr;
  u32 len = 0;
  u32 space = 0;

  // position in buffer for reading
  s32 pos = 0;

  mem::Allocator* allocator = nullptr;

  b8 open() override { return open(8); }
  b8 open(mem::Allocator* allocator) { return open(8, allocator); }
  b8 open(s32 initial_space, mem::Allocator* allocator = &mem::stl_allocator);

  void close() override;

  // Clears the buffer and resets the read position to 0.
  void clear() { len = pos = 0; }

  // Resets the position being read.
  void rewind() { pos = 0; }

  // Returns if the read position is at the end of the buffer.
  b8 atEnd() { return pos == len; }

  // Attempts to reserve 'space' bytes and returns the Bytes
  // reserved. Commit should be used to indicate how many 
  // bytes were actually used.
  Bytes reserve(s32 space);

  // Commits space reserved by reserve().
  void commit(s32 space);

  // Allocates some memory from this buffer of some type, basically equivalent
  // to reserving and then committing, but also constructs the object in place
  // too.
  template<typename T>
  T* allocateType()
  {
    Bytes reserved = reserve(sizeof(T));
    assert(reserved.len >= sizeof(T));
    commit(sizeof(T));
    new (reserved.ptr) T;
    return (T*)reserved.ptr;
  }

  u8* allocateBytes(u64 size)
  {
    Bytes reserved = reserve(size);
    assert(reserved.len == size);
    commit(size);
    mem::zero(reserved.ptr, size);
    return reserved.ptr;
  }

  u64 allocateBytesOffset(Bytes bytes)
  {
    u64 offset = len;
    u8* allocated = allocateBytes(bytes.len);
    mem::copy(allocated, bytes.ptr, bytes.len);
    return offset;
  }

  String allocateString(String s)
  {
    u8* bytes = allocateBytes(s.len);
    mem::copy(bytes, s.ptr, s.len);
    return String::from(bytes, s.len);
  }

  u64 allocateStringOffset(String s)
  {
    u64 offset = len;
    allocateString(s);
    return offset;
  }

  String asStr() { return {ptr, len}; }
  Bytes asBytes() { return {ptr, len}; }

  s64 write(Bytes slice) override;
  s64 read(Bytes slice) override;

  template<typename T>
  s64 writeType(const T& v)
  {
    return write(Bytes{ (u8*)&v, sizeof(T) });
  }

  s64 readFrom(s64 pos, Bytes slice) override;

  void set(String s)
  {
    clear();
    write(s);
  }

  // Reads from the given io until it returns no more bytes.
  s64 consume(io::IO* io, u32 chunk_size);

  /* ==========================================================================
   *  Helper for saving a position in the memory to rollback to if the user 
   *  changes their mind about writing something to the buffer. Note that 
   *  this also calls rewind() on commit.
   *
   *  Obviously this is not necessary to have an api for, but I like its 
   *  explicitness in what's going on.
   */
  typedef s64 Rollback;

  Rollback createRollback();
  void     commitRollback(Rollback rollback);

  u8& operator[](u32 x) { assert(x <= len); return ptr[x]; }

private: 
  
  void growIfNeeded(s64 space);

};

/* ============================================================================
 *  IO around a statically sized buffer.
 *  This ensures that the buffer can always be null-terminated, so it makes 
 *  room for N+1 bytes and on every write sets the byte after the write_pos 
 *  to 0.
 *
 *  Currently the buffer is always overwritten on writes. Initially this 
 *  tracked the write (and read) pos properly but that wound up being pretty
 *  useless. This is to be used for temporary stuff.
 *
 */ 
template<size_t N>
struct StaticBuffer : public IO
{
  u8  buffer[N+1];
  s32 len = 0;

  // Prevent having to call open where this is used.
  StaticBuffer<N>() { open(); }

  inline static size_t capacity() { return N; }
  String asStr() { return String{buffer, u64(len)}; }

  operator char*() { return (char*)buffer; }

  virtual s64 write(Bytes slice) override
  {
    u64 bytes_remaining = N - len;
    u64 bytes_to_write = 
      (slice.len > bytes_remaining? bytes_remaining : slice.len);
    if (bytes_to_write == 0)
      return 0;
    mem::copy(buffer+len, slice.ptr, bytes_to_write);
    len += bytes_to_write;
    buffer[len] = 0;
    return len;
  }

  virtual s64 read(Bytes slice) override
  {
    if (len)
      mem::copy(slice.ptr, buffer, len);
    return len;
  }

  s64 readFrom(io::IO* io)
  {
    len = io->read({buffer, N});
    return len;
  }

  void clear()
  {
    len = 0;
    buffer[0] = '\0';
  }
};

/* ============================================================================
 *  A buffer that lives on stack until it is overrun, at which point it moves
 *  to heap. 
 *
 *  Because this is meant for use primarily on the stack, it uses RAII to 
 *  automatically clean itself up upon exiting scope.
 *
 *  TODO(sushi) maybe support providing an allocator if ever seems useful.
 */
template<s32 StackSize>
struct SmallBuffer : public IO
{
  u8 stack[StackSize] = {};

  u8* ptr = stack;
  s32 len = 0;
  s32 space = StackSize;

  SmallBuffer() { }
  ~SmallBuffer() { clear(); }

  b8 isSmall() const { return ptr == stack; }

  String asStr() const { return String::from(ptr, len); }

  void clear()
  {
    if (!isSmall())
    {
      mem::stl_allocator.free(ptr);
      ptr = stack;
      space = StackSize;
    }
    len = 0;
    mem::zero(stack, StackSize);
  }

  s64 write(Bytes slice) override
  {
    s32 space_needed = len + slice.len;
    if (space_needed > space)
    {
      if (isSmall())
      {
        // Move to heap.
        space = space_needed * 2;
        ptr = (u8*)mem::stl_allocator.allocate(space);
        mem::copy(ptr, stack, len);
      }
      else
      {
        while (space < space_needed)
          space = 2 * space;
        ptr = (u8*)mem::stl_allocator.reallocate(ptr, space);
      }
    }

    mem::copy(ptr + len, slice.ptr, slice.len);
    len += slice.len;

    return slice.len;
  }

  s64 read(Bytes buffer) override
  {
    // TODO(sushi) implement reading from this if ever needed.
    assert(!"reading from SmallBuffer is not supported!");
    return 0;
  }
};

/* ============================================================================
 *  IO view over a String.
 */
struct StringView : public IO
{
  String s = nil;
  s32 pos = 0;

  static StringView from(String s)
  {
    StringView out;
    out.s = s;
    out.pos = 0;
    return out;
  }

  virtual s64 write(Bytes slice) override 
  {
    assert(!"cannot write to a StringView");
    return 0;
  }

  virtual s64 read(Bytes slice) override
  {
    if (pos == s.len)
      return 0;

    s64 bytes_remaining = s.len - pos;
    s64 bytes_to_read = 
      (bytes_remaining > slice.len ? slice.len : bytes_remaining);
    mem::copy(slice.ptr, s.ptr + pos, bytes_to_read);
    pos += bytes_to_read;
    return bytes_to_read;
  }
};

template<size_t N>
s64 format(IO* io, StaticBuffer<N>& x)
{
  return format(io, x.asStr());
}

}

}

DefineMove(iro::io::Memory, 
    { iro::mem::copy(&to, &from, sizeof(iro::io::Memory)); });
DefineNilValue(iro::io::Memory, {}, { return x.ptr == nullptr; });

#endif // _iro_io_h
