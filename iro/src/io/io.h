/*
 *  IO interface and various api for generic transmission of data
 */

#ifndef _iro_io_h
#define _iro_io_h

#include "common.h"
#include "unicode.h"
#include "memory/allocator.h"
#include "containers/slice.h"

#include "flags.h"
#include "assert.h"

namespace iro::io
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
	virtual s64 read_from(s64 pos, Bytes buffer) { assert(false); return 0; }

	b8 is_open()   { return flags.test(Flag::Open); }
	b8 can_read()  { return flags.test(Flag::Readable); }
	b8 can_write() { return flags.test(Flag::Writable); }

protected:

	// helpers for use internally, especially for things that define their own 'flags' like FileDescriptor
	void set_open()     { flags.set(Flag::Open); }
	void set_writable() { flags.set(Flag::Writable); }
	void set_readable() { flags.set(Flag::Readable); }

	void unset_open()     { flags.unset(Flag::Open); }
	void unset_writable() { flags.unset(Flag::Writable); }
	void unset_readable() { flags.unset(Flag::Readable); }
};

/* ================================================================================================ io::Memory
 *  IO around a memory buffer that may grow.
 *
 *  TODO(sushi) implement allocators and make this take one.
 */
struct Memory : public IO
{
	u8* buffer;
	u64 len;
	u64 space;

	// position in buffer for reading
	s64 pos;

	mem::Allocator* allocator;

	b8 open() override { return open(8); }
	b8 open(s32 initial_space, mem::Allocator* allocator = &mem::stl_allocator);

	void close() override;

	// Clears the buffer and resets the read position to 0.
	void clear() { len = pos = 0; }

	// Resets the position being read.
	void rewind() { pos = 0; }

	// Returns if the read position is at the end of the buffer.
	b8 at_end() { return pos == len; }

	// Attempts to reserve 'space' bytes and returns a pointer to 
	// the reserved memory. commit() should be used after a call 
	// to this function to indicate how many bytes were actually
	// used.
	u8* reserve(s32 space);

	// Commits space reserved by reserve().
	void commit(s32 space);

	str as_str() { return {buffer, len}; }

	s64 write(Bytes slice) override;
	s64 read(Bytes slice) override;

	s64 read_from(s64 pos, Bytes slice) override;

	/* ============================================================================================ io::Memory::Rollback
	 *  Helper for saving a position in the memory to rollback to if the user changes their mind 
	 *  about writing something to the buffer.
	 *
	 *  Obviously this is not necessary to have an api for, but I like its explicitness in what's
	 *  going on.
	 */
	typedef s64 Rollback;

	Rollback create_rollback();
	void     commit_rollback(Rollback rollback);

private: 
	
	void grow_if_needed(s64 space);

};

/* ================================================================================================ io::FileDescriptor
 *  IO around a system file descriptor.
 *
 *  Currently only supporting linux for now. 
 *  TODO(sushi) split up the implementation of this into platform layers and implement Windows.
 */
struct FileDescriptor : public IO
{
	u64 fd;

	enum class Flag
	{
		View, // the file descriptor is not one that we opened, so we will not allow using 'close' on it
		NonBlocking, // will not block on reads or writes
		Create, // if a file at the given path does not exist it will be created
	};
	typedef iro::Flags<Flag> Flags;

	Flags flags;

	[[deprecated("io::FileDescriptor must be given a file descriptor to open!")]]
	b8 open() override { return false; }

	// Creates a 'view' on an already existing file descriptor.
	// All I think I'll use this for atm is stdout/err
	// This just assumes the given fd is read/writable for now
	// TODO(sushi) figure out if we can test for read/write-ability somehow
	b8 open(u64 fd);

	b8 open(
		str path, 
		io::Flags io_flags, 
		Flags fd_flags = Flags::none());

	b8 open(
		str path,
		io::Flag io_flag,
		Flags fd_flags = Flags::none())
	{
		return open(path, io::Flags::from(io_flag), fd_flags);
	}

	b8 open(
		str path,
		io::Flag io_flag,
		Flag fd_flag)
	{
		return open(path, io::Flags::from(io_flag), Flags::from(fd_flag));
	}

	void close() override;

	virtual s64 write(Bytes slice) override;
	virtual s64 read(Bytes slice) override;
};

/* ================================================================================================ io::StaticBuffer
 *  IO around a statically sized buffer.
 *  This ensures that the buffer can always be null-terminated, so it makes room for N+1 bytes 
 *  and on every write sets the byte after the write_pos to 0.
 */
template<size_t N>
struct StaticBuffer : public IO
{
	static const size_t len = N;

	u8  buffer[N+1];
	s32 write_pos;
	s32 read_pos;

	size_t capacity() { return len; }
	str as_str() { return str{buffer, write_pos}; }

	operator char*() { return (char*)buffer; }

	void clear();
	void rewind();

	virtual s64 write(Bytes slice) override;
	virtual s64 read(Bytes slice) override;
};


};


#endif // _iro_io_h
