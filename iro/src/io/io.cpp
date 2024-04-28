#include "io.h"

#include "stdlib.h"
#include "stdio.h"
#include "memory/memory.h"

// TODO(sushi) platform layers
#include "fcntl.h"
#include "sys/stat.h"
#include "unistd.h"

#include "sys/stat.h"

#include "assert.h"

namespace iro::io
{

/* ------------------------------------------------------------------------------------------------ Memory::grow_if_needed
 */
void Memory::grow_if_needed(s64 wanted_space)
{
	if (space - len > wanted_space)
		return;

	while (space - len < wanted_space)
		space *= 2;

	buffer = (u8*)allocator->reallocate(buffer, space);
}

/* ------------------------------------------------------------------------------------------------ Memory::open
 */
b8 Memory::open(s32 initial_space, mem::Allocator* allocator)
{
	this->allocator = allocator;

	if (flags.test(Flag::Open))
		return true;

	flags.set(Flag::Writable);
	flags.set(Flag::Readable);

	space = initial_space;
	pos = len = 0;
	buffer = (u8*)allocator->allocate(space);

	if (buffer != nullptr)
	{
		flags.set(Flag::Open);
		return true;
	}
	return false;
}

/* ------------------------------------------------------------------------------------------------ Memory::close
 */
void Memory::close()
{
	if (!flags.test(Flag::Open))
		return;

	allocator->free(buffer);
	buffer = nullptr;
	len = space = 0;
	pos = 0;

	flags.unset(Flag::Open);
}

/* ------------------------------------------------------------------------------------------------ Memory::reserve
 */
u8* Memory::reserve(s32 wanted_space)
{
	grow_if_needed(wanted_space);
	
	return buffer + len;
}

/* ------------------------------------------------------------------------------------------------ Memory::commit
 */
void Memory::commit(s32 committed_space)
{
	assert(len + committed_space <= space);
	len += committed_space;
	buffer[len] = 0;
}

/* ------------------------------------------------------------------------------------------------ Memory::write
 */
s64 Memory::write(Bytes slice)
{
	grow_if_needed(slice.len);

	mem::copy(buffer + len, slice.ptr, slice.len);
	len += slice.len;

	buffer[len] = 0;

	return slice.len;
}

/* ------------------------------------------------------------------------------------------------ Memory::read
 */
s64 Memory::read(Bytes outbuffer)
{
	if (pos == len)
		return 0;

	s64 bytes_remaining = len - pos;
	s64 bytes_to_read = (bytes_remaining > outbuffer.len ? outbuffer.len : bytes_remaining);
	mem::copy(outbuffer.ptr, buffer + pos, bytes_to_read);
	pos += bytes_to_read;
	return bytes_to_read;
}

/* ------------------------------------------------------------------------------------------------ Memory::read_from
 */
s64 Memory::read_from(s64 pos, Bytes slice)
{
	assert(pos < len);

	s64 bytes_remaining = len - pos;
	s64 bytes_to_read = (bytes_remaining > slice.len ? slice.len : bytes_remaining);
	mem::copy(slice.ptr, buffer + pos, bytes_to_read);
	return bytes_to_read;
}

/* ------------------------------------------------------------------------------------------------ Memory::create_rollback
 */
Memory::Rollback Memory::create_rollback()
{
	return len;
}

/* ------------------------------------------------------------------------------------------------ Memory::commit_rollback
 */
void Memory::commit_rollback(Rollback rollback)
{
	len = rollback;
}

/* ------------------------------------------------------------------------------------------------ FileDescriptor::open
 */
b8 FileDescriptor::open(u64 fd_)
{
	if (is_open())
		return true;

	fd = fd_;

	// TODO(sushi) set these based on if we're given stdin/out/err
	set_writable();
	set_readable();
	set_open();

	return true;
}

/* ------------------------------------------------------------------------------------------------ FileDescriptor::open
 */
b8 FileDescriptor::open(str path, io::Flags io_flags, Flags fd_flags)
{
	using enum io::Flag;
	if (is_open())
		return true;

	if (!io_flags.test_any<Writable, Readable>())
		return false;

	static const u32 max_path_size = 255;
	u8 ntbuf[max_path_size];
	if (!path.null_terminate(ntbuf, max_path_size))
		return false;

	int flags = 0;

	if (io_flags.test_all<Writable, Readable>())
	{
		flags |= O_RDWR;
		set_writable();
		set_readable();
	}
	else if (io_flags.test(Readable))
	{
		flags |= O_RDONLY;
		set_readable();
	}
	else if (io_flags.test(Writable))
	{
		flags |= O_WRONLY;
		set_writable();
	}

	if (fd_flags.test(Flag::NonBlocking))
		flags |= O_NONBLOCK;

	if (fd_flags.test(Flag::Create))
		flags |= O_CREAT;

	fd = ::open((char*)ntbuf, flags, S_IRUSR | S_IWUSR);

	if (fd == -1)
	{
		perror("FileDescriptor::open() in call to ::open()");
		return false;
	}

	set_open();
	return true;
}

/* ------------------------------------------------------------------------------------------------ FileDescriptor::close
 */
void FileDescriptor::close()
{
	using enum io::Flag;

	if (!is_open() || flags.test(Flag::View))
		return;

	if (::close(fd) == -1)
	{
		perror("FileDescriptor::close() in call to ::close()");
	}

	unset_open();
}

/* ------------------------------------------------------------------------------------------------ FileDescriptor::write
 */
s64 FileDescriptor::write(Bytes slice)
{
	if (!is_open() || !can_write())
		return 0;

	int r = ::write(fd, slice.ptr, slice.len);

	if (r == -1)
	{
		perror("FileDescriptor::write() in call to ::write()");
		return 0;
	}
	return r;
}

/* ------------------------------------------------------------------------------------------------ FileDescriptor::read
 */
s64 FileDescriptor::read(Bytes slice)
{
	if (!is_open() || !can_read())
		return 0;

	int r = ::read(fd, slice.ptr, slice.len);

	if (r == -1)
	{
		perror("FileDescriptor::read() in call to ::read()");
		return 0;
	}

	return r;
}

/* ------------------------------------------------------------------------------------------------ StaticBuffer::clear
 */
template<size_t N>
void StaticBuffer<N>::clear()
{
	write_pos = read_pos = 0;
}

/* ------------------------------------------------------------------------------------------------ StaticBuffer::rewind
 */
template<size_t N>
void StaticBuffer<N>::rewind()
{
	read_pos = 0;
}

/* ------------------------------------------------------------------------------------------------ StaticBuffer::write
 */
template<size_t N>
s64 StaticBuffer<N>::write(Bytes slice)
{
	if (write_pos == len)
		return 0;

	s64 space_remaining = len - write_pos;
	s64 bytes_to_write = (slice.len > space_remaining ? space_remaining : slice.len);
	mem::copy(buffer + write_pos, slice.ptr, bytes_to_write);
	write_pos += bytes_to_write;
	buffer[write_pos] = 0;
	return bytes_to_write;
}

/* ------------------------------------------------------------------------------------------------ StaticBuffer::read
 */
template<size_t N>
s64 StaticBuffer<N>::read(Bytes slice)
{
	if (read_pos == write_pos)
		return 0;

	s64 space_remaining = write_pos - read_pos;
	s64 bytes_to_read = (slice.len < space_remaining ? slice.len : space_remaining);
	mem::copy(slice.ptr, buffer + read_pos, bytes_to_read);
	read_pos += bytes_to_read;
	return bytes_to_read;
}

} 
