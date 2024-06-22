/*
 *  IO interface and various api for generic transmission of data
 */

#ifndef _iro_io_h
#define _iro_io_h

#include "../common.h"
#include "../unicode.h"
#include "../flags.h"

#include "../traits/nil.h"
#include "../traits/move.h"
#include "../memory/allocator.h"
#include "../memory/memory.h"
#include "../containers/slice.h"

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
};

typedef iro::Flags<Flag> Flags; 

/* ================================================================================================ io::IO
 *  Base interface for input/output
 *
 *  TODO(sushi) try to make a trait that handles this behavior so we don't have to use vtables to 
 *              support this. Though I guess then we lose dynamic dispatch? I dunno, maybe traits
 *          	could do something with virtual stuff to move the vtables off of the actual 
 *          	structures implementing the behavior? I just dont like how this makes me need 
 *          	to use 'new' but there's probably no escaping that.
 */
struct IO
{
	Flags flags = {};

	// Writes the contents of 'slice' into this IO and returns the number of bytes written.
	virtual s64 write(Bytes slice) = 0;

	// Reads the contents of this IO into the buffer at 'buffer.bytes' up to 'buffer.len' and 
	// returns the number of bytes read.
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

protected:

	// helpers for use internally, especially for things that define their own 'flags' 
	// like FileDescriptor
	void setOpen()     { flags.set(Flag::Open); }
	void setWritable() { flags.set(Flag::Writable); }
	void setReadable() { flags.set(Flag::Readable); }

	void unsetOpen()     { flags.unset(Flag::Open); }
	void unsetWritable() { flags.unset(Flag::Writable); }
	void unsetReadable() { flags.unset(Flag::Readable); }
};

/* ================================================================================================ io::Memory
 *  IO around a memory buffer that may grow.
 */
struct Memory : public IO
{
	u8* buffer = nullptr;
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

	str asStr() { return {buffer, len}; }

	s64 write(Bytes slice) override;
	s64 read(Bytes slice) override;

	s64 readFrom(s64 pos, Bytes slice) override;

	/* ============================================================================================ io::Memory::Rollback
	 *  Helper for saving a position in the memory to rollback to if the user changes their mind 
	 *  about writing something to the buffer. Note that this also calls rewind() on commit.
	 *
	 *  Obviously this is not necessary to have an api for, but I like its explicitness in what's
	 *  going on.
	 */
	typedef s64 Rollback;

	Rollback createRollback();
	void     commitRollback(Rollback rollback);

	u8& operator[](u32 x) { assert(x <= len); return buffer[x]; }

private: 
	
	void growIfNeeded(s64 space);

};

/* ================================================================================================ io::StaticBuffer
 *  IO around a statically sized buffer.
 *  This ensures that the buffer can always be null-terminated, so it makes room for N+1 bytes 
 *  and on every write sets the byte after the write_pos to 0.
 *
 *  TODO(sushi) this sucks and ive found it to be useless in most cases. Either get rid of this
 *              or fix it so its not so useless.
 */ 
template<size_t N>
struct StaticBuffer : public IO
{
	static const size_t len = N;

	u8  buffer[N+1];
	s32 write_pos;
	s32 read_pos;

	size_t capacity() { return len; }
	str asStr() { return str{buffer, u64(write_pos)}; }

	operator char*() { return (char*)buffer; }

	void clear();
	void rewind();

	virtual s64 write(Bytes slice) override;
	virtual s64 read(Bytes slice) override;
};

}

}

DefineMove(iro::io::Memory, { iro::mem::copy(&to, &from, sizeof(iro::io::Memory)); });
DefineNilValue(iro::io::Memory, {}, { return x.buffer == nullptr; });

#endif // _iro_io_h
